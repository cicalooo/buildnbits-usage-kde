#include "UsagePopup.h"
#include "MenuWidget.h"

#include <QCursor>
#include <QWidgetAction>

UsagePopup::UsagePopup(QWidget *parent)
    : QMenu(parent)
{
    setObjectName(QStringLiteral("usagePopup"));
    setWindowFlag(Qt::FramelessWindowHint, true);

    auto *cardsAction = new QWidgetAction(this);
    m_cards = new MenuWidget(this);
    cardsAction->setDefaultWidget(m_cards);
    addAction(cardsAction);
    addSeparator();

    addAction(tr("Refresh"), this, &UsagePopup::refreshRequested);
    addAction(tr("Settings"), this, &UsagePopup::settingsRequested);
    addSeparator();
    addAction(tr("Quit"), this, &UsagePopup::quitRequested);
    connect(this, &QMenu::aboutToHide, this, [this]() {
        if (m_anchor)
            m_anchor->hide();
    });
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
    QPoint pos = globalPos;
    if (pos.isNull())
        pos = QCursor::pos();

    if (!m_anchor) {
        m_anchor = new QWidget(nullptr, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowDoesNotAcceptFocus
                                            | Qt::BypassWindowManagerHint);
        m_anchor->setAttribute(Qt::WA_ShowWithoutActivating);
        m_anchor->setAttribute(Qt::WA_TranslucentBackground);
        m_anchor->setFixedSize(1, 1);
    }
    m_anchor->move(pos);
    m_anchor->show();
    m_anchor->winId();
    popup(m_anchor->mapToGlobal(QPoint(0, 0)));
}
