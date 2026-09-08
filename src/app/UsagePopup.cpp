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
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
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
    root->setContentsMargins(10, 10, 10, 10);
    root->setSpacing(8);

    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_cards = new MenuWidget(scroll);
    scroll->setWidget(m_cards);
    root->addWidget(scroll, 1);

    auto *row = new QHBoxLayout();
    auto *refresh = new QPushButton(tr("Refresh"), this);
    auto *settings = new QPushButton(tr("Settings"), this);
    auto *quit = new QPushButton(tr("Quit"), this);
    connect(refresh, &QPushButton::clicked, this, &UsagePopup::refreshRequested);
    connect(settings, &QPushButton::clicked, this, &UsagePopup::settingsRequested);
    connect(quit, &QPushButton::clicked, this, &UsagePopup::quitRequested);
    row->addWidget(refresh);
    row->addWidget(settings);
    row->addStretch();
    row->addWidget(quit);
    root->addLayout(row);

    setMinimumWidth(380);
    setMaximumHeight(720);
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
    QSize sz = m_cards->sizeHint().expandedTo(QSize(380, 220));
    sz.setWidth(qBound(380, sz.width(), avail.width() - 16));
    sz.setHeight(qBound(220, sz.height() + 52, qMin(720, avail.height() - 24)));
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
