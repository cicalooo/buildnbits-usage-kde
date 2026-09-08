#pragma once

#include <QWidget>
#include <QVector>

class QHBoxLayout;
class Provider;

class MenuWidget : public QWidget {
    Q_OBJECT
public:
    explicit MenuWidget(QWidget *parent = nullptr);
    void updateData(const QVector<Provider *> &providers);
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

private:
    QWidget *createCard(Provider *provider);
    void clearCards();
    QHBoxLayout *m_cardsLayout;
    int m_visibleCount = 0;
};
