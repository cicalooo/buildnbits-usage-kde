#include "TrayIcon.h"

#include "UsagePopup.h"

#include <KLocalizedString>
#include <KWindowSystem>
#include <algorithm>
#include <QAction>
#include <QApplication>
#include <QCoreApplication>
#include <QCursor>
#include <QDesktopServices>
#include <QGuiApplication>
#include <QMenu>
#include <QPalette>
#include <QScreen>
#include <QSettings>
#include <QUrl>

static QString sniIdFor(ProviderID id) {
    switch (id) {
    case ProviderID::Codex:
        return QStringLiteral("buildnbits-usage-codex");
    case ProviderID::Grok:
        return QStringLiteral("buildnbits-usage-grok");
    case ProviderID::Antigravity:
        return QStringLiteral("buildnbits-usage-agy");
    default:
        return QStringLiteral("buildnbits-usage");
    }
}

TrayIcon::TrayIcon(ProviderRegistry *registry, QObject *parent)
    : QObject(parent)
    , m_registry(registry)
    , m_timer(new QTimer(this))
    , m_popup(new UsagePopup())
{
    connect(m_popup, &UsagePopup::settingsRequested, this, &TrayIcon::openSettings);
    connect(m_popup, &UsagePopup::refreshRequested, this, &TrayIcon::refreshEnabled);
    connect(m_popup, &UsagePopup::quitRequested, qApp, &QCoreApplication::quit);

    QSettings s(QStringLiteral("BuildnBits"), QStringLiteral("Usage"));
    for (auto *provider : m_registry->providers()) {
        m_windowIndex.insert(provider->id(), s.value(QStringLiteral("windowIndex/%1").arg(static_cast<int>(provider->id())), 0).toInt());
        connect(provider, &Provider::dataChanged, this, &TrayIcon::updateIcons);
        connect(provider, &Provider::stateChanged, this, [this](ProviderState) { updateIcons(); });
    }

    connect(m_timer, &QTimer::timeout, this, &TrayIcon::refreshEnabled);
    const int interval = s.value(QStringLiteral("refresh_interval"), 300000).toInt();
    if (interval > 0)
        m_timer->start(interval);

    rebuildItems();
    QTimer::singleShot(0, this, &TrayIcon::refreshEnabled);
}

bool TrayIcon::isEnabled(ProviderID id) const {
    QSettings s(QStringLiteral("BuildnBits"), QStringLiteral("Usage"));
    switch (id) {
    case ProviderID::Codex:
        return s.value(QStringLiteral("showCodex"), true).toBool();
    case ProviderID::Grok:
        return s.value(QStringLiteral("showGrok"), true).toBool();
    case ProviderID::Antigravity:
        return s.value(QStringLiteral("showAgy"), true).toBool();
    default:
        return false;
    }
}

QVector<Provider *> TrayIcon::enabledProviders() const {
    QVector<Provider *> result;
    const ProviderID tracked[] = {ProviderID::Codex, ProviderID::Grok, ProviderID::Antigravity};
    for (ProviderID id : tracked) {
        if (!isEnabled(id))
            continue;
        if (auto *provider = m_registry->provider(id))
            result.append(provider);
    }
    return result;
}

void TrayIcon::refreshEnabled() {
    for (auto *provider : enabledProviders())
        provider->refresh();
}

qreal TrayIcon::devicePixelRatio() const {
    if (QGuiApplication::primaryScreen())
        return QGuiApplication::primaryScreen()->devicePixelRatio();
    return 1.0;
}

SquarePaint TrayIcon::paintFor(Provider *provider, int windowIndex) const {
    SquarePaint paint;
    paint.brand = IconRenderer::brandFor(provider->id());
    const QColor window = QApplication::palette().color(QPalette::Window);
    paint.dark = qGray(window.rgb()) < 140;
    paint.stale = provider->state() == ProviderState::Stale;
    paint.signedOut = provider->state() == ProviderState::SignedOut;
    paint.error = provider->state() == ProviderState::Error && provider->snapshot().limits.isEmpty();

    const auto limits = provider->snapshot().limits;
    if (limits.isEmpty() || paint.signedOut) {
        paint.value = -1;
        return paint;
    }

    QList<int> order;
    for (int i = 0; i < limits.size(); ++i)
        order.append(i);
    std::sort(order.begin(), order.end(), [&](int a, int b) {
        return limits.at(a).displayPercent() < limits.at(b).displayPercent();
    });
    const int idx = order.at(qBound(0, windowIndex, order.size() - 1));
    paint.value = qBound(0, qRound(limits.at(idx).displayPercent()), 100);
    paint.usedForHeat = qBound(0, qRound(limits.at(idx).usedPercent()), 100);
    return paint;
}

void TrayIcon::paintItem(Item &item) {
    auto *provider = m_registry->provider(item.id);
    if (!provider || !item.sni)
        return;

    const SquarePaint paint = paintFor(provider, item.windowIndex);
    item.sni->setIconByPixmap(IconRenderer::renderSquare(paint, devicePixelRatio()).pixmap(22, 22));

    QString tooltip = QStringLiteral("<b>%1</b>").arg(provider->name());
    const auto snap = provider->snapshot();
    if (provider->state() == ProviderState::SignedOut) {
        tooltip += QStringLiteral("<br>Not signed in");
    } else if (snap.limits.isEmpty()) {
        tooltip += QStringLiteral("<br>No usage data");
    } else {
        for (const auto &limit : snap.limits) {
            tooltip += QStringLiteral("<br>%1: %2%")
                           .arg(limit.label)
                           .arg(qRound(limit.displayPercent()));
            if (!limit.resetDescription.isEmpty())
                tooltip += QStringLiteral(" · %1").arg(limit.resetDescription);
        }
        if (paint.stale)
            tooltip += QStringLiteral("<br>cached");
    }
    item.sni->setToolTip(QIcon(), provider->name(), tooltip);
}

void TrayIcon::cycleWindow(Item &item, int delta) {
    auto *provider = m_registry->provider(item.id);
    if (!provider)
        return;
    const int n = provider->snapshot().limits.size();
    if (n <= 1)
        return;
    item.windowIndex = (item.windowIndex + delta) % n;
    if (item.windowIndex < 0)
        item.windowIndex += n;
    m_windowIndex[item.id] = item.windowIndex;
    QSettings(QStringLiteral("BuildnBits"), QStringLiteral("Usage"))
        .setValue(QStringLiteral("windowIndex/%1").arg(static_cast<int>(item.id)), item.windowIndex);
    paintItem(item);
}

void TrayIcon::openSettings() {
    if (!m_settingsDialog) {
        m_settingsDialog = new SettingsDialog();
        connect(m_settingsDialog, &SettingsDialog::settingsChanged, this, &TrayIcon::applySettings);
    }
    m_settingsDialog->show();
    m_settingsDialog->raise();
    m_settingsDialog->activateWindow();
}

void TrayIcon::showPopup(const QPoint &pos) {
    m_popup->setProviders(enabledProviders());
    m_popup->toggleAt(pos);
}

QMenu *TrayIcon::buildContextMenu() {
    auto *menu = new QMenu();
    auto *open = menu->addAction(i18n("Open usage"));
    connect(open, &QAction::triggered, this, [this]() { showPopup(QCursor::pos()); });
    menu->addSeparator();
    auto *settings = menu->addAction(i18n("Settings"));
    connect(settings, &QAction::triggered, this, &TrayIcon::openSettings);
    menu->addAction(i18n("Refresh All"), this, &TrayIcon::refreshEnabled);
    menu->addSeparator();
    auto *about = menu->addAction(QStringLiteral("BuildnBits Usage %1").arg(QCoreApplication::applicationVersion()));
    connect(about, &QAction::triggered, this, []() {
        QDesktopServices::openUrl(QUrl(QStringLiteral("https://github.com/cicalooo/buildnbits-usage-kde")));
    });
    menu->addSeparator();
    menu->addAction(QIcon::fromTheme(QStringLiteral("application-exit")), i18n("Quit"),
                    qApp, &QCoreApplication::quit);
    return menu;
}

void TrayIcon::rebuildItems() {
    for (auto &item : m_items) {
        delete item.sni;
        delete item.menu;
    }
    m_items.clear();

    for (auto *provider : enabledProviders()) {
        Item item;
        item.id = provider->id();
        item.windowIndex = m_windowIndex.value(provider->id(), 0);
        item.sni = new KStatusNotifierItem(sniIdFor(provider->id()), this);
        item.sni->setCategory(KStatusNotifierItem::ApplicationStatus);
        item.sni->setStatus(KStatusNotifierItem::Active);
        item.sni->setStandardActionsEnabled(false);
        item.sni->setTitle(provider->name());
        item.menu = buildContextMenu();
        item.sni->setContextMenu(item.menu);

        connect(item.sni, &KStatusNotifierItem::activateRequested, this,
                [this, sni = item.sni](bool active, const QPoint &pos) {
                    const QString token = sni->providedToken();
                    if (!token.isEmpty())
                        KWindowSystem::setCurrentXdgActivationToken(token);
                    if (!active) {
                        m_popup->hide();
                        return;
                    }
                    showPopup(pos);
                });
        connect(item.sni, &KStatusNotifierItem::secondaryActivateRequested, this,
                [this, id = item.id](const QPoint &) {
                    for (auto &it : m_items) {
                        if (it.id == id)
                            cycleWindow(it, 1);
                    }
                });
        connect(item.sni, &KStatusNotifierItem::scrollRequested, this, [this, id = item.id](int delta, Qt::Orientation) {
            for (auto &it : m_items) {
                if (it.id == id)
                    cycleWindow(it, delta > 0 ? 1 : -1);
            }
        });

        m_items.append(item);
        paintItem(m_items.last());
    }
}

void TrayIcon::updateIcons() {
    for (auto &item : m_items)
        paintItem(item);
}

void TrayIcon::applySettings() {
    if (!m_settingsDialog)
        return;
    const int interval = m_settingsDialog->refreshInterval();
    if (interval > 0)
        m_timer->start(interval);
    else
        m_timer->stop();
    rebuildItems();
    refreshEnabled();
}
