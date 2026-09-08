#pragma once

#include "Provider.h"
#include <QProcess>

class AntigravityProvider : public Provider {
    Q_OBJECT
public:
    explicit AntigravityProvider(QObject *parent = nullptr);
    ~AntigravityProvider() override;

    void refresh() override;

private slots:
    void onFinished(int exitCode, QProcess::ExitStatus status);
    void onError(QProcess::ProcessError error);

private:
    void parseOutput(const QByteArray &json);
    QProcess *m_process = nullptr;
    bool m_isFetching = false;
};
