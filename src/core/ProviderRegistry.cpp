#include "ProviderRegistry.h"
#include "CodexProvider.h"
#include "AntigravityProvider.h"
#include "GrokProvider.h"

ProviderRegistry::ProviderRegistry(QObject *parent) : QObject(parent)
{
    registerProvider(new CodexProvider(this));
    registerProvider(new GrokProvider(this));
    registerProvider(new AntigravityProvider(this));
}

void ProviderRegistry::registerProvider(Provider *provider) {
    if (provider) {
        provider->setParent(this);
        m_providers.append(provider);
    }
}

QVector<Provider *> ProviderRegistry::providers() const { return m_providers; }

Provider *ProviderRegistry::provider(ProviderID id) const {
    for (auto *p : m_providers) {
        if (p->id() == id)
            return p;
    }
    return nullptr;
}
