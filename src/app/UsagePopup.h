#pragma once

#include <QWidget>
#include <QVector>
#include <QElapsedTimer>

class MenuWidget;
class Provider;

class UsagePopup : public QWidget {
    Q_OBJECT
public:
    explicit UsagePopup(QWidget *parent = nullptr);

    void setProviders(const QVector<Provider *> &providers);
    void toggleAt(const QPoint &globalPos);

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

    MenuWidget *m_cards;
    QElapsedTimer m_shownAt;
    bool m_layerConfigured = false;
};
