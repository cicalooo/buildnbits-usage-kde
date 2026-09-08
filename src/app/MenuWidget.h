#pragma once

#include <QWidget>
#include <QVector>

class QVBoxLayout;
class Provider;

class MenuWidget : public QWidget {
    Q_OBJECT
public:
    explicit MenuWidget(QWidget *parent = nullptr);
    void updateData(const QVector<Provider *> &providers);
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

private:
    QWidget *createCard(Provider *provider, bool expanded);
    void clearCards();
    QVBoxLayout *m_cardsLayout;
    int m_visibleCount = 0;
    int m_barCount = 1;
};
