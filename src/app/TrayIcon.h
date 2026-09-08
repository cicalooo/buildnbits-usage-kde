#pragma once

#include <KStatusNotifierItem>
#include <QHash>
#include <QObject>
#include <QTimer>

#include "IconRenderer.h"
#include "ProviderRegistry.h"
#include "SettingsDialog.h"

class QMenu;
class QWidgetAction;
class MenuWidget;

class TrayIcon : public QObject {
    Q_OBJECT
public:
    explicit TrayIcon(ProviderRegistry *registry, QObject *parent = nullptr);

private slots:
    void updateIcons();
    void applySettings();

private:
    struct Item {
        ProviderID id = ProviderID::Unknown;
        KStatusNotifierItem *sni = nullptr;
        QMenu *menu = nullptr;
        MenuWidget *menuWidget = nullptr;
        int windowIndex = 0;
    };

    void rebuildItems();
    void refreshEnabled();
    QVector<Provider *> enabledProviders() const;
    bool isEnabled(ProviderID id) const;
    void cycleWindow(Item &item, int delta);
    void paintItem(Item &item);
    SquarePaint paintFor(Provider *provider, int windowIndex) const;
    qreal devicePixelRatio() const;

    ProviderRegistry *m_registry;
    QTimer *m_timer;
    SettingsDialog *m_settingsDialog = nullptr;
    QList<Item> m_items;
    QHash<ProviderID, int> m_windowIndex;
};
