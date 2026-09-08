#pragma once

#include "Provider.h"
#include <QProcess>
#include <QTimer>

class QJsonObject;

class GrokProvider : public Provider {
    Q_OBJECT
public:
    explicit GrokProvider(QObject *parent = nullptr);
    ~GrokProvider() override;

    void refresh() override;

private slots:
    void onProcessStarted();
    void onReadyReadStandardOutput();
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);
    void onProcessError(QProcess::ProcessError error);
    void onTimeout();

private:
    enum class State { Idle, Starting, Initializing, FetchingBilling, Finished };

    void send(const QJsonObject &payload);
    void handleMessage(const QJsonObject &message);
    void handleBilling(const QJsonObject &billing);
    void finishError();

    QProcess *m_process = nullptr;
    QTimer m_timeout;
    QByteArray m_buffer;
    State m_state = State::Idle;
    int m_nextId = 1;
    int m_initializeId = -1;
    int m_billingId = -1;
};
