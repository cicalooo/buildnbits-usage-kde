#include "GrokProvider.h"

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

void GrokProvider::refresh() {
    if (m_state != State::Idle && m_state != State::Finished)
        return;

    if (QStandardPaths::findExecutable(QStringLiteral("grok")).isEmpty()) {
        markUnavailable();
        return;
    }

    if (m_process) {
        m_process->deleteLater();
        m_process = nullptr;
    }
    m_buffer.clear();
    m_state = State::Starting;
    m_timeout.start(12000);

    m_process = new QProcess(this);
    connect(m_process, &QProcess::started, this, &GrokProvider::onProcessStarted);
    connect(m_process, &QProcess::readyReadStandardOutput, this,
            &GrokProvider::onReadyReadStandardOutput);
    connect(m_process, &QProcess::finished, this, &GrokProvider::onProcessFinished);
    connect(m_process, &QProcess::errorOccurred, this, &GrokProvider::onProcessError);
    m_process->start(QStringLiteral("grok"), {QStringLiteral("--no-auto-update"), QStringLiteral("agent"), QStringLiteral("stdio")});
}

void GrokProvider::onProcessStarted() {
    m_state = State::Initializing;
    QJsonObject capabilities;
    capabilities[QStringLiteral("fs")] = QJsonObject{{QStringLiteral("readTextFile"), false}, {QStringLiteral("writeTextFile"), false}};
    capabilities[QStringLiteral("terminal")] = false;
    QJsonObject params{{QStringLiteral("protocolVersion"), QStringLiteral("1")}, {QStringLiteral("clientCapabilities"), capabilities}};
    m_initializeId = m_nextId++;
    send(QJsonObject{{QStringLiteral("jsonrpc"), QStringLiteral("2.0")}, {QStringLiteral("id"), m_initializeId},
                     {QStringLiteral("method"), QStringLiteral("initialize")}, {QStringLiteral("params"), params}});
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
    if (message.value(QStringLiteral("id")).toInt() == m_initializeId && message.contains(QStringLiteral("result"))) {
        m_state = State::FetchingBilling;
        m_billingId = m_nextId++;
        send(QJsonObject{{QStringLiteral("jsonrpc"), QStringLiteral("2.0")}, {QStringLiteral("id"), m_billingId},
                         {QStringLiteral("method"), QStringLiteral("x.ai/billing")}, {QStringLiteral("params"), QJsonObject{}}});
    } else if (message.value(QStringLiteral("id")).toInt() == m_billingId && message.contains(QStringLiteral("result"))) {
        handleBilling(message.value(QStringLiteral("result")).toObject());
    } else if (message.value(QStringLiteral("id")).toInt() == m_billingId && message.contains(QStringLiteral("error"))) {
        finishError();
    }
}

void GrokProvider::handleBilling(const QJsonObject &billing) {
    QJsonObject root = billing;
    if (billing.value(QStringLiteral("config")).isObject())
        root = billing.value(QStringLiteral("config")).toObject();

    double usedPercent = -1;
    if (root.contains(QStringLiteral("creditUsagePercent")))
        usedPercent = root.value(QStringLiteral("creditUsagePercent")).toDouble(-1);
    if (usedPercent < 0) {
        const auto usedObj = root.value(QStringLiteral("usage")).toObject().value(QStringLiteral("totalUsed")).toObject();
        const double used = usedObj.value(QStringLiteral("val")).toDouble();
        const double limit = root.value(QStringLiteral("monthlyLimit")).toObject().value(QStringLiteral("val")).toDouble();
        if (limit > 0)
            usedPercent = (used / limit) * 100.0;
    }
    if (usedPercent < 0) {
        finishError();
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
        reset = root.value(QStringLiteral("billingCycle")).toObject().value(QStringLiteral("billingPeriodEnd")).toString();
    if (reset.isEmpty())
        reset = billing.value(QStringLiteral("billingCycle")).toObject().value(QStringLiteral("billingPeriodEnd")).toString();
    const QDateTime resetTime = QDateTime::fromString(reset, Qt::ISODate);
    if (resetTime.isValid())
        credits.resetDescription = resetTime.toLocalTime().toString(QStringLiteral("yyyy-MM-dd hh:mm"));
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
