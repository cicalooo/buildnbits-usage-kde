#pragma once

#include <QString>
#include <QDateTime>
#include <QList>
#include <QObject>
#include <QtGlobal>

enum class ProviderID {
    Codex,
    Claude,
    Gemini,
    Antigravity,
    Grok,
    Unknown
};

enum class ProviderState {
    Active,
    Error,
    Stale,
    SignedOut
};

struct UsageLimit {
    QString label;
    double used = 0.0;
    double total = 0.0;
    QString unit;
    QString resetDescription;
    int durationMinutes = 0;
    bool valid = true;
    bool displayRemaining = true;

    double usedPercent() const {
        if (total <= 0) return 0.0;
        return (used / total) * 100.0;
    }

    double percent() const { return usedPercent(); }

    double displayPercent() const {
        const double p = usedPercent();
        return displayRemaining ? qBound(0.0, 100.0 - p, 100.0) : p;
    }
};

struct UsageSnapshot {
    QList<UsageLimit> limits;
    QDateTime timestamp;
    QString source;
};

class Provider : public QObject {
    Q_OBJECT
public:
    explicit Provider(ProviderID id, QObject *parent = nullptr);
    virtual ~Provider() = default;

    ProviderID id() const;
    QString name() const;

    ProviderState state() const;
    UsageSnapshot snapshot() const;

    virtual void refresh() = 0;

signals:
    void dataChanged();
    void stateChanged(ProviderState newState);

protected:
    void setSnapshot(const UsageSnapshot &snapshot);
    void setState(ProviderState state);
    void markUnavailable();
    void markSignedOut();

private:
    void loadLastGood();
    void saveLastGood() const;

    ProviderID m_id;
    ProviderState m_state;
    UsageSnapshot m_snapshot;
};
