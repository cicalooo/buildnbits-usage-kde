#include "AntigravityProvider.h"
#include "Format.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QTimer>

AntigravityProvider::AntigravityProvider(QObject *parent)
    : Provider(ProviderID::Antigravity, parent) {}

AntigravityProvider::~AntigravityProvider() {
    if (m_process)
        m_process->kill();
}

void AntigravityProvider::refresh() {
    if (m_isFetching)
        return;
    if (QStandardPaths::findExecutable(QStringLiteral("agy")).isEmpty()) {
        markUnavailable();
        return;
    }

    m_isFetching = true;
    if (m_process) {
        m_process->deleteLater();
        m_process = nullptr;
    }
    m_process = new QProcess(this);
    m_process->setProcessChannelMode(QProcess::SeparateChannels);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &AntigravityProvider::onFinished);
    connect(m_process, &QProcess::errorOccurred, this, &AntigravityProvider::onError);
    QTimer::singleShot(15000, this, [this]() {
        if (m_isFetching && m_process && m_process->state() != QProcess::NotRunning)
            m_process->kill();
    });
    m_process->start(QStringLiteral("agy"), {QStringLiteral("-p"), QStringLiteral("/usage"),
                                             QStringLiteral("--output-format"), QStringLiteral("json")});
}

void AntigravityProvider::onError(QProcess::ProcessError) {
    m_isFetching = false;
    markUnavailable();
}

void AntigravityProvider::onFinished(int exitCode, QProcess::ExitStatus) {
    m_isFetching = false;
    if (exitCode != 0) {
        markUnavailable();
        return;
    }
    parseOutput(m_process ? m_process->readAllStandardOutput() : QByteArray());
}

static QJsonObject unwrapData(QJsonObject root) {
    if (root.value(QStringLiteral("command")).isObject()) {
        const QJsonObject command = root.value(QStringLiteral("command")).toObject();
        if (command.value(QStringLiteral("data")).isObject())
            return command.value(QStringLiteral("data")).toObject();
        return command;
    }
    if (root.value(QStringLiteral("data")).isObject())
        return root.value(QStringLiteral("data")).toObject();
    return root;
}

static bool readUsage(const QJsonObject &bucket, double *used, double *remaining) {
    auto num = [&](const char *a, const char *b, bool *ok) {
        *ok = true;
        if (bucket.contains(QLatin1String(a)))
            return bucket.value(QLatin1String(a)).toDouble();
        if (bucket.contains(QLatin1String(b)))
            return bucket.value(QLatin1String(b)).toDouble();
        *ok = false;
        return 0.0;
    };
    bool ok = false;
    double remainingFraction = num("remaining_fraction", "remainingFraction", &ok);
    if (ok) {
        *remaining = remainingFraction <= 1.0 ? remainingFraction * 100.0 : remainingFraction;
        *used = 100.0 - *remaining;
        return true;
    }
    double remainingPercent = num("remaining_percent", "remainingPercent", &ok);
    if (ok) {
        *remaining = remainingPercent;
        *used = 100.0 - *remaining;
        return true;
    }
    double usedFraction = num("used_fraction", "usedFraction", &ok);
    if (ok) {
        *used = usedFraction <= 1.0 ? usedFraction * 100.0 : usedFraction;
        *remaining = 100.0 - *used;
        return true;
    }
    double usedPercent = num("used_percent", "usedPercent", &ok);
    if (ok) {
        *used = usedPercent;
        *remaining = 100.0 - *used;
        return true;
    }
    return false;
}

void AntigravityProvider::parseOutput(const QByteArray &json) {
    const QJsonObject root = QJsonDocument::fromJson(json).object();
    const QString status = root.value(QStringLiteral("status")).toString();
    if (!status.isEmpty() && status.compare(QStringLiteral("SUCCESS"), Qt::CaseInsensitive) != 0) {
        markUnavailable();
        return;
    }
    const QJsonObject data = unwrapData(root);
    const QJsonArray groups = data.value(QStringLiteral("groups")).toArray();
    UsageSnapshot snap;
    snap.timestamp = QDateTime::currentDateTime();
    snap.source = QStringLiteral("local");
    for (const auto &g : groups) {
        const QJsonObject group = g.toObject();
        const QString groupName = group.value(QStringLiteral("name")).toString(QStringLiteral("Model quota"));
        for (const auto &b : group.value(QStringLiteral("buckets")).toArray()) {
            const QJsonObject bucket = b.toObject();
            double used = 0, remaining = 0;
            if (!readUsage(bucket, &used, &remaining))
                continue;
            UsageLimit limit;
            const QString bucketName = bucket.value(QStringLiteral("name")).toString(
                bucket.value(QStringLiteral("id")).toString(QStringLiteral("Usage")));
            limit.label = groupName.compare(bucketName, Qt::CaseInsensitive) == 0
                              ? groupName
                              : groupName + QStringLiteral(" · ") + bucketName;
            limit.used = qBound(0.0, used, 100.0);
            limit.total = 100.0;
            limit.displayRemaining = true;
            const QString window = bucket.value(QStringLiteral("window")).toString();
            const QString hay = (window + QLatin1Char(' ') + bucketName + QLatin1Char(' ') + groupName).toLower();
            if (hay.contains(QLatin1String("week")))
                limit.durationMinutes = 10080;
            else if (hay.contains(QLatin1String("5h")) || hay.contains(QLatin1String("5-hour"))
                     || hay.contains(QLatin1String("five")) || hay.contains(QLatin1String("hour")))
                limit.durationMinutes = 300;
            QString reset = bucket.value(QStringLiteral("reset_time")).toString();
            if (reset.isEmpty())
                reset = bucket.value(QStringLiteral("resetTime")).toString();
            limit.resetAt = parseIsoDateTime(reset);
            snap.limits.append(limit);
        }
    }
    if (snap.limits.isEmpty()) {
        markUnavailable();
        return;
    }
    sortWindowsShortFirst(&snap.limits);
    setSnapshot(snap);
    setState(ProviderState::Active);
}
