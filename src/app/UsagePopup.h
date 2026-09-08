#pragma once

#include "Provider.h"
#include <QElapsedTimer>
#include <QVector>
#include <QWidget>

class MenuWidget;

class UsagePopup : public QWidget {
    Q_OBJECT
public:
    explicit UsagePopup(QWidget *parent = nullptr);

    void setProviders(const QVector<Provider *> &providers);
    void toggleProvider(Provider *provider, const QPoint &globalPos);

signals:
    void settingsRequested();
    void refreshRequested();
    void quitRequested();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    void applyLayerShell(const QPoint &globalPos, const QSize &size);
    void placeOnX11(const QPoint &globalPos, const QSize &size);

    void presentAt(const QPoint &globalPos);

    MenuWidget *m_cards;
    QElapsedTimer m_shownAt;
    ProviderID m_openId = ProviderID::Unknown;
    bool m_layerConfigured = false;
};
