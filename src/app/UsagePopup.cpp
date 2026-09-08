#include "UsagePopup.h"
#include "MenuWidget.h"

#include <QApplication>
#include <QCursor>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QFrame>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QVBoxLayout>

UsagePopup::UsagePopup(QWidget *parent)
    : QWidget(parent, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint)
{
    setObjectName(QStringLiteral("usagePopup"));
    setAttribute(Qt::WA_ShowWithoutActivating, false);
    setWindowTitle(tr("BuildnBits Usage"));

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
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

    setMinimumWidth(320);
    setMaximumHeight(640);
}

void UsagePopup::setProviders(const QVector<Provider *> &providers) {
    m_cards->updateData(providers);
    adjustSize();
}

void UsagePopup::toggleAt(const QPoint &globalPos) {
    if (isVisible()) {
        hide();
        return;
    }
    adjustSize();
    QPoint pos = globalPos;
    if (pos.isNull())
        pos = QCursor::pos();
    QScreen *screen = QGuiApplication::screenAt(pos);
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    QRect avail = screen ? screen->availableGeometry() : QRect(0, 0, 800, 600);
    QSize sz = sizeHint().expandedTo(minimumSize());
    sz.setWidth(qMin(sz.width(), avail.width() - 16));
    sz.setHeight(qMin(qMax(sz.height(), 200), qMin(640, avail.height() - 16)));
    resize(sz);
    int x = pos.x() - sz.width() / 2;
    int y = pos.y() - sz.height() - 8;
    if (y < avail.top())
        y = pos.y() + 8;
    x = qBound(avail.left() + 4, x, avail.right() - sz.width() - 4);
    y = qBound(avail.top() + 4, y, avail.bottom() - sz.height() - 4);
    move(x, y);
    show();
    raise();
    activateWindow();
}
