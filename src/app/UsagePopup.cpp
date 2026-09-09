#include "UsagePopup.h"
#include "MenuWidget.h"

#include <LayerShellQt/Window>

#include <QCursor>
#include <QEvent>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QIcon>
#include <QScreen>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWindow>

UsagePopup::UsagePopup(QWidget *parent)
    : QWidget(parent, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool)
{
    setObjectName(QStringLiteral("usagePopup"));
    setAttribute(Qt::WA_DeleteOnClose, false);
    setWindowTitle(QString());
    setFocusPolicy(Qt::StrongFocus);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(8, 6, 8, 8);
    root->setSpacing(4);

    auto iconButton = [this](const QString &iconName, const QString &fallback, const QString &tip) {
        auto *btn = new QToolButton(this);
        QIcon icon = QIcon::fromTheme(iconName);
        if (icon.isNull())
            icon = QIcon::fromTheme(fallback);
        btn->setIcon(icon);
        btn->setIconSize(QSize(16, 16));
        btn->setAutoRaise(true);
        btn->setFocusPolicy(Qt::NoFocus);
        btn->setToolTip(tip);
        btn->setFixedSize(24, 24);
        btn->setStyleSheet(QStringLiteral(
            "QToolButton { border: none; border-radius: 4px; padding: 2px; }"
            "QToolButton:hover { background: palette(mid); }"
            "QToolButton:pressed { background: palette(dark); }"));
        return btn;
    };

    auto *toolbar = new QHBoxLayout();
    toolbar->setContentsMargins(0, 0, 0, 0);
    toolbar->setSpacing(2);
    auto *refresh = iconButton(QStringLiteral("view-refresh"), QStringLiteral("view-refresh-symbolic"), tr("Refresh"));
    auto *settings = iconButton(QStringLiteral("settings-configure"), QStringLiteral("configure"), tr("Settings"));
    auto *closeBtn = iconButton(QStringLiteral("window-close"), QStringLiteral("dialog-close"), tr("Close"));
    connect(refresh, &QToolButton::clicked, this, &UsagePopup::refreshRequested);
    connect(settings, &QToolButton::clicked, this, &UsagePopup::settingsRequested);
    connect(closeBtn, &QToolButton::clicked, this, &UsagePopup::hide);
    toolbar->addWidget(refresh);
    toolbar->addWidget(settings);
    toolbar->addStretch();
    toolbar->addWidget(closeBtn);
    root->addLayout(toolbar);

    m_cards = new MenuWidget(this);
    root->addWidget(m_cards, 1);

    setMinimumWidth(360);
    setMaximumWidth(420);
    qApp->installEventFilter(this);
}

void UsagePopup::setProviders(const QVector<Provider *> &providers) {
    m_cards->updateData(providers);
    adjustSize();
}

void UsagePopup::applyLayerShell(const QPoint &globalPos, const QSize &size) {
    winId();
    QWindow *wh = windowHandle();
    if (!wh)
        return;
    auto *layer = LayerShellQt::Window::get(wh);
    if (!layer)
        return;

    QScreen *screen = QGuiApplication::screenAt(globalPos);
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    const QRect geo = screen ? screen->geometry() : QRect(0, 0, 1920, 1080);

    const bool bottom = globalPos.y() > geo.center().y();
    int left = globalPos.x() - size.width() / 2 - geo.left();
    left = qBound(8, left, geo.width() - size.width() - 8);
    const int edge = 8;

    layer->setLayer(LayerShellQt::Window::LayerTop);
    layer->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityOnDemand);
    layer->setScope(QStringLiteral("buildnbits-usage"));
    layer->setDesiredSize(size);
    layer->setWantsToBeOnActiveScreen(false);
    if (screen)
        layer->setScreen(screen);
    if (bottom) {
        layer->setAnchors(LayerShellQt::Window::Anchors(LayerShellQt::Window::AnchorLeft)
                          | LayerShellQt::Window::AnchorBottom);
        layer->setMargins(QMargins(left, 0, 0, edge));
    } else {
        layer->setAnchors(LayerShellQt::Window::Anchors(LayerShellQt::Window::AnchorLeft)
                          | LayerShellQt::Window::AnchorTop);
        layer->setMargins(QMargins(left, edge, 0, 0));
    }
    m_layerConfigured = true;
}

void UsagePopup::placeOnX11(const QPoint &globalPos, const QSize &size) {
    QScreen *screen = QGuiApplication::screenAt(globalPos);
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    const QRect avail = screen ? screen->availableGeometry() : QRect(0, 0, 800, 600);
    int x = globalPos.x() - size.width() / 2;
    int y = globalPos.y() - size.height() - 8;
    if (y < avail.top())
        y = globalPos.y() + 12;
    x = qBound(avail.left() + 4, x, avail.right() - size.width() - 4);
    y = qBound(avail.top() + 4, y, avail.bottom() - size.height() - 4);
    setGeometry(QRect(QPoint(x, y), size));
}

void UsagePopup::toggleProvider(Provider *provider, const QPoint &globalPos) {
    if (!provider)
        return;
    if (isVisible() && m_openId == provider->id()) {
        if (m_shownAt.isValid() && m_shownAt.elapsed() < 250)
            return;
        hide();
        m_openId = ProviderID::Unknown;
        return;
    }
    if (isVisible())
        hide();
    setProviders({provider});
    m_openId = provider->id();
    presentAt(globalPos);
}

void UsagePopup::presentAt(const QPoint &globalPos) {
    QPoint pos = globalPos;
    if (pos.isNull())
        pos = QCursor::pos();

    QScreen *screen = QGuiApplication::screenAt(pos);
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    const QRect avail = screen ? screen->availableGeometry() : QRect(0, 0, 800, 600);
    QSize sz = sizeHint().expandedTo(QSize(360, 120));
    sz.setWidth(qBound(360, sz.width(), qMin(420, avail.width() - 16)));
    sz.setHeight(qBound(120, sz.height(), avail.height() - 24));
    resize(sz);

    const bool wayland = QGuiApplication::platformName() == QLatin1String("wayland");
    if (wayland)
        applyLayerShell(pos, sz);
    else
        placeOnX11(pos, sz);

    show();
    raise();
    m_shownAt.start();
}

bool UsagePopup::eventFilter(QObject *watched, QEvent *event) {
    Q_UNUSED(watched);
    if (!isVisible())
        return false;
    if (event->type() == QEvent::MouseButtonPress) {
        const auto *mouse = static_cast<QMouseEvent *>(event);
        const QPoint g = mouse->globalPosition().toPoint();
        if (!frameGeometry().contains(g) && m_shownAt.isValid() && m_shownAt.elapsed() > 250)
            hide();
    }
    return false;
}

void UsagePopup::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Escape)
        hide();
    else
        QWidget::keyPressEvent(event);
}
