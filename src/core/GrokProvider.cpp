#include "GrokProvider.h"
#include "Format.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

GrokProvider::GrokProvider(QObject *parent)
    : Provider(ProviderID::Grok, parent) {
    m_timeout.setSingleShot(true);
    connect(&m_timeout, &QTimer::timeout, this, &GrokProvider::onTimeout);
}

GrokProvider::~GrokProvider() {
    if (m_process)
        m_process->kill();
}

QString GrokProvider::findGrok() {
    const QString onPath = QStandardPaths::findExecutable(QStringLiteral("grok"));
    if (!onPath.isEmpty())
        return onPath;
    const QString home = qEnvironmentVariable(QByteArrayLiteral("GROK_HOME"),
                                              QDir::homePath() + QStringLiteral("/.grok"));
    const QString candidate = home + QStringLiteral("/bin/grok");
    if (QFileInfo::exists(candidate) && QFileInfo(candidate).isExecutable())
        return candidate;
    return {};
}

void GrokProvider::refresh() {
    if (m_state != State::Idle && m_state != State::Finished)
        return;

    const QString grok = findGrok();
    if (grok.isEmpty()) {
        markUnavailable();
        return;
    }

    if (m_process) {
        m_process->deleteLater();
        m_process = nullptr;
    }
    m_buffer.clear();
    m_billingAttempt = 0;
    m_billingMethods = {QStringLiteral("_x.ai/billing"), QStringLiteral("x.ai/billing")};
    m_state = State::Starting;
    m_timeout.start(20000);

    m_process = new QProcess(this);
    m_process->setWorkingDirectory(QDir::homePath());
    connect(m_process, &QProcess::started, this, &GrokProvider::onProcessStarted);
    connect(m_process, &QProcess::readyReadStandardOutput, this,
            &GrokProvider::onReadyReadStandardOutput);
    connect(m_process, &QProcess::readyReadStandardError, this, [this]() {
        if (m_process)
            m_process->readAllStandardError();
    });
    connect(m_process, &QProcess::finished, this, &GrokProvider::onProcessFinished);
    connect(m_process, &QProcess::errorOccurred, this, &GrokProvider::onProcessError);
    m_process->start(grok, {QStringLiteral("--no-auto-update"), QStringLiteral("agent"),
                            QStringLiteral("stdio")});
}

void GrokProvider::onProcessStarted() {
    m_state = State::Initializing;
    QJsonObject fs{{QStringLiteral("readTextFile"), false}, {QStringLiteral("writeTextFile"), false}};
    QJsonObject clientCapabilities{{QStringLiteral("fs"), fs}, {QStringLiteral("terminal"), false}};
    QJsonObject clientInfo{{QStringLiteral("name"), QStringLiteral("BuildnBits.Usage")},
                           {QStringLiteral("version"), QStringLiteral("0.1.0")}};
    QJsonObject params;
    params[QStringLiteral("protocolVersion")] = 1;
    params[QStringLiteral("clientInfo")] = clientInfo;
    params[QStringLiteral("capabilities")] = QJsonObject();
    params[QStringLiteral("clientCapabilities")] = clientCapabilities;
    m_initializeId = m_nextId++;
    send(QJsonObject{{QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
                     {QStringLiteral("id"), m_initializeId},
                     {QStringLiteral("method"), QStringLiteral("initialize")},
                     {QStringLiteral("params"), params}});
}

void GrokProvider::send(const QJsonObject &payload) {
    if (!m_process)
        return;
    m_process->write(QJsonDocument(payload).toJson(QJsonDocument::Compact));
    m_process->write("\n");
}

void GrokProvider::onReadyReadStandardOutput() {
    if (!m_process)
        return;
    m_buffer += m_process->readAllStandardOutput();
    while (true) {
        const int newline = m_buffer.indexOf('\n');
        if (newline < 0)
            break;
        const QByteArray line = m_buffer.left(newline).trimmed();
        m_buffer.remove(0, newline + 1);
        if (line.isEmpty())
            continue;
        QJsonParseError error;
        const QJsonDocument document = QJsonDocument::fromJson(line, &error);
        if (error.error == QJsonParseError::NoError && document.isObject())
            handleMessage(document.object());
    }
}

void GrokProvider::handleMessage(const QJsonObject &message) {
    const int id = message.value(QStringLiteral("id")).toInt(-1);
    if (id < 0)
        return; // notifications such as _x.ai/announcements/update

    if (id == m_initializeId && message.contains(QStringLiteral("result"))) {
        handleInitialize(message.value(QStringLiteral("result")).toObject());
        return;
    }
    if (id == m_authId) {
        if (message.contains(QStringLiteral("error"))) {
            markSignedOut();
            m_state = State::Finished;
            m_timeout.stop();
            if (m_process)
                m_process->terminate();
            return;
        }
        requestNextBilling();
        return;
    }
    if (id == m_billingId) {
        if (message.contains(QStringLiteral("error"))) {
            requestNextBilling();
            return;
        }
        handleBilling(message.value(QStringLiteral("result")).toObject());
    }
}

void GrokProvider::handleInitialize(const QJsonObject &result) {
    bool hasCachedToken = false;
    for (const auto &v : result.value(QStringLiteral("authMethods")).toArray()) {
        if (v.toObject().value(QStringLiteral("id")).toString() == QStringLiteral("cached_token")) {
            hasCachedToken = true;
            break;
        }
    }
    if (!hasCachedToken) {
        markSignedOut();
        m_state = State::Finished;
        m_timeout.stop();
        if (m_process)
            m_process->terminate();
        return;
    }

    send(QJsonObject{{QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
                     {QStringLiteral("method"), QStringLiteral("initialized")},
                     {QStringLiteral("params"), QJsonObject()}});

    m_state = State::Authenticating;
    m_authId = m_nextId++;
    QJsonObject authParams{{QStringLiteral("methodId"), QStringLiteral("cached_token")},
                           {QStringLiteral("authMethodId"), QStringLiteral("cached_token")}};
    send(QJsonObject{{QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
                     {QStringLiteral("id"), m_authId},
                     {QStringLiteral("method"), QStringLiteral("authenticate")},
                     {QStringLiteral("params"), authParams}});
}

void GrokProvider::requestNextBilling() {
    if (m_billingAttempt >= m_billingMethods.size()) {
        finishError();
        return;
    }
    m_state = State::FetchingBilling;
    m_billingId = m_nextId++;
    const QString method = m_billingMethods.at(m_billingAttempt++);
    send(QJsonObject{{QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
                     {QStringLiteral("id"), m_billingId},
                     {QStringLiteral("method"), method},
                     {QStringLiteral("params"), QJsonObject()}});
}

void GrokProvider::handleBilling(const QJsonObject &billing) {
    QJsonObject root = billing;
    if (billing.value(QStringLiteral("config")).isObject())
        root = billing.value(QStringLiteral("config")).toObject();

    double usedPercent = -1;
    if (root.contains(QStringLiteral("creditUsagePercent")))
        usedPercent = root.value(QStringLiteral("creditUsagePercent")).toDouble(-1);
    if (usedPercent < 0) {
        const double used = root.value(QStringLiteral("usage")).toObject()
                                .value(QStringLiteral("totalUsed")).toObject()
                                .value(QStringLiteral("val")).toDouble();
        const double limit = root.value(QStringLiteral("monthlyLimit")).toObject()
                                 .value(QStringLiteral("val")).toDouble();
        if (limit > 0)
            usedPercent = (used / limit) * 100.0;
    }
    if (usedPercent < 0) {
        requestNextBilling();
        return;
    }

    UsageSnapshot snapshot;
    snapshot.timestamp = QDateTime::currentDateTime();
    snapshot.source = QStringLiteral("oauth");
    UsageLimit credits;
    credits.label = QStringLiteral("Weekly");
    credits.used = qBound(0.0, usedPercent, 100.0);
    credits.total = 100.0;
    credits.displayRemaining = true;
    credits.durationMinutes = 10080;
    QString reset = root.value(QStringLiteral("currentPeriod")).toObject().value(QStringLiteral("end")).toString();
    if (reset.isEmpty())
        reset = root.value(QStringLiteral("billingPeriodEnd")).toString();
    credits.resetAt = parseIsoDateTime(reset);
    snapshot.limits.append(credits);
    m_timeout.stop();
    m_state = State::Finished;
    setSnapshot(snapshot);
    setState(ProviderState::Active);
    if (m_process)
        m_process->terminate();
}

void GrokProvider::finishError() {
    m_timeout.stop();
    m_state = State::Finished;
    markUnavailable();
    if (m_process)
        m_process->terminate();
}

void GrokProvider::onProcessFinished(int, QProcess::ExitStatus) {
    if (m_state != State::Finished)
        finishError();
}

void GrokProvider::onProcessError(QProcess::ProcessError) {
    if (m_state != State::Finished)
        finishError();
}

void GrokProvider::onTimeout() {
    if (m_process)
        m_process->kill();
    if (m_state != State::Finished)
        finishError();
}
