#include "UsageCache.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

static QString keyFor(ProviderID id) {
    switch (id) {
    case ProviderID::Codex:
        return QStringLiteral("codex");
    case ProviderID::Grok:
        return QStringLiteral("grok");
    case ProviderID::Antigravity:
        return QStringLiteral("agy");
    default:
        return QStringLiteral("unknown");
    }
}

QString UsageCache::rootDir() {
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
        + QStringLiteral("/BuildnBits/Usage");
}

QString UsageCache::cachePath() {
    return rootDir() + QStringLiteral("/usage-cache.json");
}

static QJsonObject readDoc() {
    QFile file(UsageCache::cachePath());
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return QJsonDocument::fromJson(file.readAll()).object();
}

static void writeDoc(const QJsonObject &obj) {
    QDir().mkpath(UsageCache::rootDir());
    QFile file(UsageCache::cachePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;
    file.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
}

void UsageCache::save(ProviderID id, const UsageSnapshot &snapshot) {
    if (snapshot.limits.isEmpty())
        return;
    QJsonObject doc = readDoc();
    QJsonObject entry;
    entry[QStringLiteral("source")] = snapshot.source;
    entry[QStringLiteral("ts")] = snapshot.timestamp.toUTC().toString(Qt::ISODate);
    QJsonArray windows;
    for (const auto &limit : snapshot.limits) {
        QJsonObject w;
        w[QStringLiteral("label")] = limit.label;
        w[QStringLiteral("used")] = limit.used;
        w[QStringLiteral("total")] = limit.total;
        w[QStringLiteral("remaining")] = limit.displayPercent();
        w[QStringLiteral("reset")] = limit.resetDescription;
        w[QStringLiteral("durationMinutes")] = limit.durationMinutes;
        windows.append(w);
    }
    entry[QStringLiteral("windows")] = windows;
    doc[keyFor(id)] = entry;
    writeDoc(doc);
}

bool UsageCache::load(ProviderID id, UsageSnapshot *snapshot) {
    const QJsonObject entry = readDoc().value(keyFor(id)).toObject();
    if (entry.isEmpty())
        return false;
    snapshot->source = entry.value(QStringLiteral("source")).toString();
    snapshot->timestamp = QDateTime::fromString(entry.value(QStringLiteral("ts")).toString(), Qt::ISODate);
    const QJsonArray windows = entry.value(QStringLiteral("windows")).toArray();
    snapshot->limits.clear();
    for (const auto &v : windows) {
        const QJsonObject w = v.toObject();
        UsageLimit limit;
        limit.label = w.value(QStringLiteral("label")).toString();
        limit.used = w.value(QStringLiteral("used")).toDouble();
        limit.total = w.value(QStringLiteral("total")).toDouble(100.0);
        limit.resetDescription = w.value(QStringLiteral("reset")).toString();
        limit.durationMinutes = w.value(QStringLiteral("durationMinutes")).toInt();
        limit.displayRemaining = true;
        limit.valid = limit.total > 0;
        if (limit.valid)
            snapshot->limits.append(limit);
    }
    return !snapshot->limits.isEmpty();
}
