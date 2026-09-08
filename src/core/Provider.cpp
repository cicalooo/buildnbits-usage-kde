#include "Provider.h"
#include "UsageCache.h"

Provider::Provider(ProviderID id, QObject *parent)
    : QObject(parent), m_id(id), m_state(ProviderState::Stale) {
    loadLastGood();
}

ProviderID Provider::id() const { return m_id; }

QString Provider::name() const {
    switch (m_id) {
    case ProviderID::Codex:
        return "Codex";
    case ProviderID::Claude:
        return "Claude";
    case ProviderID::Gemini:
        return "Gemini";
    case ProviderID::Antigravity:
        return "Antigravity";
    case ProviderID::Grok:
        return "Grok";
    default:
        return "Unknown";
    }
}

ProviderState Provider::state() const { return m_state; }

UsageSnapshot Provider::snapshot() const { return m_snapshot; }

void Provider::setSnapshot(const UsageSnapshot &snapshot) {
    m_snapshot = snapshot;
    if (!snapshot.limits.isEmpty())
        saveLastGood();
    emit dataChanged();
}

void Provider::setState(ProviderState state) {
    if (m_state != state) {
        m_state = state;
        emit stateChanged(state);
    }
}

void Provider::markUnavailable() {
    if (!m_snapshot.limits.isEmpty())
        setState(ProviderState::Stale);
    else
        setState(ProviderState::Error);
}

void Provider::markSignedOut() {
    setState(ProviderState::SignedOut);
    emit dataChanged();
}

void Provider::loadLastGood() {
    UsageSnapshot snap;
    if (UsageCache::load(m_id, &snap)) {
        m_snapshot = snap;
        m_state = ProviderState::Stale;
    }
}

void Provider::saveLastGood() const {
    UsageCache::save(m_id, m_snapshot);
}
