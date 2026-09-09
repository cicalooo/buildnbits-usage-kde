#pragma once

#include "Provider.h"

#include <algorithm>

inline QDateTime parseIsoDateTime(const QString &text) {
    if (text.isEmpty())
        return {};
    QDateTime dt = QDateTime::fromString(text, Qt::ISODateWithMs);
    if (!dt.isValid())
        dt = QDateTime::fromString(text, Qt::ISODate);
    if (dt.isValid() && dt.timeSpec() != Qt::LocalTime)
        dt = dt.toLocalTime();
    return dt;
}

inline QString formatResetCountdown(const QDateTime &resetAt) {
    if (!resetAt.isValid())
        return {};
    qint64 secs = QDateTime::currentDateTime().secsTo(resetAt);
    if (secs <= 0)
        return QStringLiteral("resetting");
    const qint64 days = secs / 86400;
    const qint64 hours = (secs % 86400) / 3600;
    const qint64 mins = (secs % 3600) / 60;
    if (days >= 1) {
        return QStringLiteral("%1:%2:%3")
            .arg(days, 2, 10, QChar('0'))
            .arg(hours, 2, 10, QChar('0'))
            .arg(mins, 2, 10, QChar('0'));
    }
    if (hours > 0)
        return QStringLiteral("%1h %2m").arg(hours).arg(mins);
    return QStringLiteral("%1m").arg(qMax<qint64>(1, mins));
}

inline QString resetLabel(const UsageLimit &limit) {
    if (limit.resetAt.isValid())
        return formatResetCountdown(limit.resetAt);
    return limit.resetDescription;
}

inline int windowSortKey(const UsageLimit &limit) {
    if (limit.durationMinutes > 0)
        return limit.durationMinutes;
    const QString s = limit.label.toLower();
    if (s.contains(QLatin1String("5-hour")) || s.contains(QLatin1String("5h"))
        || (s.contains(QLatin1String("hour")) && !s.contains(QLatin1String("week"))))
        return 300;
    if (s.contains(QLatin1String("week")) || s.contains(QLatin1String("7-day"))
        || s.contains(QLatin1String("7d")))
        return 10080;
    return 50000;
}

inline void sortWindowsShortFirst(QList<UsageLimit> *limits) {
    std::sort(limits->begin(), limits->end(), [](const UsageLimit &a, const UsageLimit &b) {
        return windowSortKey(a) < windowSortKey(b);
    });
}
