#include "CodexProvider.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>

CodexProvider::CodexProvider(QObject *parent)
    : Provider(ProviderID::Codex, parent)
{
}

CodexProvider::~CodexProvider()
{
    if (m_process) {
        m_process->kill();
        m_process->deleteLater();
    }
}

static bool hasCodexOAuth() {
    QFile file(QDir::homePath() + QStringLiteral("/.codex/auth.json"));
    if (!file.open(QIODevice::ReadOnly))
        return false;
    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    const QJsonObject tokens = root.value(QStringLiteral("tokens")).toObject();
    const QString access = tokens.value(QStringLiteral("access_token")).toString();
    if (!access.isEmpty())
        return true;
    // ChatGPT login also stores the token at the root in some CLI versions.
    return !root.value(QStringLiteral("access_token")).toString().isEmpty();
}

void CodexProvider::refresh()
{
    if (!hasCodexOAuth()) {
        markSignedOut();
        m_internalState = State::Idle;
        return;
    }

    if (m_internalState != State::Idle && m_internalState != State::Finished) {
        // Already running
        return;
    }

    if (m_process) {
        m_process->deleteLater();
        m_process = nullptr;
    }

    m_process = new QProcess(this);
    connect(m_process, &QProcess::started, this, &CodexProvider::onProcessStarted);
    connect(m_process, &QProcess::readyReadStandardOutput, this, &CodexProvider::onReadyReadStandardOutput);
    connect(m_process, &QProcess::finished, this, &CodexProvider::onProcessFinished);
    connect(m_process, &QProcess::errorOccurred, this, &CodexProvider::onProcessError);

    m_internalState = State::Starting;
    setState(ProviderState::Active); 
    // We could set a Loading state if we had one, but Active is fine.
    
    // Run the app server without write access or approval prompts.
    QString program = "codex";
    QStringList arguments;
    arguments << "--sandbox" << "read-only"
              << "--ask-for-approval" << "never"
              << "app-server";
    
    // TODO: Ideally resolve 'codex' path similar to macOS BinaryLocator
    // For now rely on PATH.
    
    m_process->start(program, arguments);
}

void CodexProvider::onProcessStarted()
{
    m_internalState = State::Initializing;
    
    // Send initialize
    // params: ["clientInfo": ["name": clientName, "version": clientVersion]]    qDebug() << "CodexProvider: Process started, sending initialize";
    
    QJsonObject clientInfo;
    clientInfo["name"] = "codexbar-linux";
    clientInfo["version"] = "2.0.0";
    
    QJsonObject params;
    params["clientInfo"] = clientInfo;
    
    m_initializeId = m_nextId++;
    QJsonObject request;
    request["id"] = m_initializeId;
    request["method"] = "initialize";
    request["params"] = params;
    
    sendPayload(request);
}

void CodexProvider::onReadyReadStandardOutput()
{
    if (!m_process) return;
    m_buffer.append(m_process->readAllStandardOutput());
    
    while (true) {
        int newlineIndex = m_buffer.indexOf('\n');
        if (newlineIndex == -1) break;
        
        QByteArray line = m_buffer.left(newlineIndex);
        m_buffer.remove(0, newlineIndex + 1);
        
        if (line.trimmed().isEmpty()) continue;
        
        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(line, &parseError);
        if (parseError.error == QJsonParseError::NoError && doc.isObject()) {
            handleMessage(doc.object());
        } else {
            qWarning() << "CodexProvider: Failed to parse JSON:" << line;
        }
    }
}

void CodexProvider::handleMessage(const QJsonObject &message)
{
    if (message.contains("id")) {
        int id = message["id"].toInt();
        if (message.contains("result")) {
            handleRpcResult(id, message["result"]);
        } else if (message.contains("error")) {
            QJsonObject err = message["error"].toObject();
            qWarning() << "CodexProvider: RPC Error:" << err["message"].toString();
            // TODO: Handle error state
        }
    } else {
        // Notification or other message
        // macOS ignores notifications usually: if message["id"] == nil ...
    }
}

void CodexProvider::handleRpcResult(int id, const QJsonValue &result)
{
    if (id == m_initializeId) {
        qDebug() << "CodexProvider: Initialized, fetching limits";
        // Initialized. Now send "initialized" notification and then fetch limits.
        sendNotification("initialized");
        
        m_fetchLimitsId = m_nextId++;
        QJsonObject request;
        request["id"] = m_fetchLimitsId;
        request["method"] = "account/rateLimits/read";
        
        sendPayload(request);
        m_internalState = State::FetchingLimits;
        
    } else if (id == m_fetchLimitsId) {
        // Parse rate limits
        // Response structure: { rateLimits: { primary: {...}, secondary: {...}, credits: {...} } }
        UsageSnapshot snapshot;
        snapshot.timestamp = QDateTime::currentDateTime();
        
        QJsonObject root = result.toObject(); // "result" value passed in
        QJsonObject rateLimits = root["rateLimits"].toObject();
        
        // Helper to parse window
        auto parseWindow = [](const QString &label, const QJsonObject &win) -> UsageLimit {
            UsageLimit limit;
            limit.label = label;
            limit.valid = false;
            if (win.isEmpty() || !win.contains(QStringLiteral("usedPercent")))
                return limit;

            limit.valid = true;
            limit.used = win[QStringLiteral("usedPercent")].toDouble();
            limit.total = 100.0;
            limit.unit = QStringLiteral("%");
            limit.displayRemaining = true;
            limit.durationMinutes = win[QStringLiteral("windowDurationMins")].toInt();
            if (limit.durationMinutes == 300)
                limit.label = QStringLiteral("5-hour");
            else if (limit.durationMinutes == 10080)
                limit.label = QStringLiteral("7-day");
            const qint64 resetsAt = static_cast<qint64>(win[QStringLiteral("resetsAt")].toDouble());
            if (resetsAt > 0) {
                const qint64 secondsLeft = QDateTime::currentSecsSinceEpoch() < resetsAt
                    ? resetsAt - QDateTime::currentSecsSinceEpoch() : 0;
                const qint64 hours = secondsLeft / 3600;
                const qint64 minutes = (secondsLeft % 3600) / 60;
                limit.resetDescription = hours > 0
                    ? QString("Resets in %1h %2m").arg(hours).arg(minutes)
                    : QString("Resets in %1m").arg(minutes);
            } else {
                limit.resetDescription = win[QStringLiteral("resetDescription")].toString();
            }
            return limit;
        };

        snapshot.source = QStringLiteral("cli");
        const UsageLimit session = parseWindow("Session", rateLimits[QStringLiteral("primary")].toObject());
        const UsageLimit weekly = parseWindow("Weekly", rateLimits[QStringLiteral("secondary")].toObject());
        if (session.valid)
            snapshot.limits.append(session);
        if (weekly.valid)
            snapshot.limits.append(weekly);
        
        qDebug() << "CodexProvider: Fetched limits. Count:" << snapshot.limits.size();

        setSnapshot(snapshot);
        setState(ProviderState::Active);
        
        m_internalState = State::Finished;
        
        // Done for this refresh cycle
        m_process->terminate();
    }
}

void CodexProvider::sendPayload(const QJsonObject &payload)
{
    if (!m_process) return;
    QJsonDocument doc(payload);
    QByteArray data = doc.toJson(QJsonDocument::Compact);
    m_process->write(data);
    m_process->write("\n");
}

void CodexProvider::sendNotification(const QString &method, const QJsonObject &params)
{
    QJsonObject p;
    p["method"] = method;
    if (!params.isEmpty()) {
        p["params"] = params;
    }
    sendPayload(p);
}

void CodexProvider::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    Q_UNUSED(exitCode);
    // If we already finished logically (e.g. successful fetch), ignore the exit status of the killed process
    if (m_internalState == State::Finished) return;

    m_internalState = State::Finished;
    if (exitStatus == QProcess::CrashExit) {
         qWarning() << "CodexProvider: Process crashed";
         markUnavailable();
    }
    // else normal exit
}

void CodexProvider::onProcessError(QProcess::ProcessError error)
{
    // If we're done, ignore errors (like Crashed due to terminate())
    if (m_internalState == State::Finished) return;

    qWarning() << "CodexProvider: Process error" << error;
    markUnavailable();
    m_internalState = State::Idle;
}
