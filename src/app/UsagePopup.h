#pragma once

#include <QMenu>
#include <QVector>

class MenuWidget;
class Provider;

class UsagePopup : public QMenu {
    Q_OBJECT
public:
    explicit UsagePopup(QWidget *parent = nullptr);

    void setProviders(const QVector<Provider *> &providers);
    void toggleAt(const QPoint &globalPos);

signals:
    void settingsRequested();
    void refreshRequested();
    void quitRequested();

private:
    MenuWidget *m_cards;
    QWidget *m_anchor = nullptr;
};
