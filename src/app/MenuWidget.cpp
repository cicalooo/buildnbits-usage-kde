#include "MenuWidget.h"
#include "Format.h"
#include "IconRenderer.h"
#include "Provider.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QVBoxLayout>

MenuWidget::MenuWidget(QWidget *parent) : QWidget(parent), m_cardsLayout(new QVBoxLayout(this)) {
    m_cardsLayout->setContentsMargins(0, 0, 0, 0);
    m_cardsLayout->setSpacing(0);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
}

void MenuWidget::clearCards() {
    while (QLayoutItem *item = m_cardsLayout->takeAt(0)) {
        if (item->widget())
            delete item->widget();
        delete item;
    }
}

QWidget *MenuWidget::createCard(Provider *provider, bool expanded) {
    Q_UNUSED(expanded);
    const QColor brand = IconRenderer::brandFor(provider->id());
    auto *card = new QFrame(this);
    card->setObjectName(QStringLiteral("squareCard"));
    card->setFrameShape(QFrame::NoFrame);
    card->setStyleSheet(QStringLiteral(
        "QFrame#squareCard { background: transparent; }"
        "QLabel { border: none; background: transparent; }"));

    auto *outer = new QHBoxLayout(card);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(8);

    auto *rail = new QFrame(card);
    rail->setFixedWidth(3);
    rail->setStyleSheet(QStringLiteral("background: %1; border: none; border-radius: 2px;")
                            .arg(brand.name()));
    outer->addWidget(rail);

    auto *layout = new QVBoxLayout();
    layout->setContentsMargins(0, 0, 2, 2);
    layout->setSpacing(4);
    outer->addLayout(layout, 1);

    auto snapshot = provider->snapshot();
    sortWindowsShortFirst(&snapshot.limits);

    auto *header = new QHBoxLayout();
    header->setSpacing(6);
    auto *title = new QLabel(provider->name(), card);
    QFont titleFont = title->font();
    titleFont.setBold(true);
    title->setFont(titleFont);
    title->setStyleSheet(QStringLiteral("color: %1;").arg(brand.name()));
    header->addWidget(title);
    header->addStretch();

    const ProviderState state = provider->state();
    if (state == ProviderState::Stale) {
        auto *stale = new QLabel(tr("cached"), card);
        stale->setStyleSheet(QStringLiteral("color: palette(mid); font-size: 10px;"));
        header->addWidget(stale);
    }
    layout->addLayout(header);

    if (state == ProviderState::SignedOut) {
        auto *hint = new QLabel(provider->id() == ProviderID::Codex
                                    ? tr("Sign in with Codex CLI")
                                    : tr("Not signed in"),
                                card);
        hint->setStyleSheet(QStringLiteral("color: palette(mid);"));
        layout->addWidget(hint);
        return card;
    }

    if (snapshot.limits.isEmpty()) {
        auto *status = new QLabel(state == ProviderState::Error ? tr("Unavailable") : tr("Waiting for data"), card);
        status->setStyleSheet(QStringLiteral("color: palette(mid);"));
        layout->addWidget(status);
        return card;
    }

    for (const auto &limit : snapshot.limits) {
        auto *row = new QHBoxLayout();
        row->setSpacing(8);
        auto *name = new QLabel(limit.label, card);
        name->setStyleSheet(QStringLiteral("font-size: 11px;"));
        row->addWidget(name, 1);
        auto *pct = new QLabel(QStringLiteral("%1%").arg(qRound(limit.displayPercent())), card);
        pct->setStyleSheet(QStringLiteral("font-weight: 600; color: %1;").arg(brand.name()));
        row->addWidget(pct);
        const QString reset = resetLabel(limit);
        auto *eta = new QLabel(reset.isEmpty() ? QStringLiteral("—") : reset, card);
        eta->setStyleSheet(QStringLiteral("color: palette(mid); font-size: 11px; font-family: monospace;"));
        eta->setMinimumWidth(64);
        eta->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        row->addWidget(eta);
        layout->addLayout(row);

        auto *bar = new QProgressBar(card);
        bar->setRange(0, 100);
        bar->setValue(qBound(0, qRound(limit.displayPercent()), 100));
        bar->setTextVisible(false);
        bar->setFixedHeight(5);
        bar->setStyleSheet(QStringLiteral(
            "QProgressBar { background: rgba(127,127,127,28); border: none; border-radius: 2px; max-height: 5px; }"
            "QProgressBar::chunk { background: %1; border-radius: 2px; }")
                               .arg(brand.name()));
        layout->addWidget(bar);
    }
    return card;
}

void MenuWidget::updateData(const QVector<Provider *> &providers) {
    clearCards();
    m_visibleCount = providers.size();
    m_barCount = 1;
    const bool expanded = providers.size() == 1;
    for (auto *provider : providers) {
        m_barCount = qMax(m_barCount, provider->snapshot().limits.size());
        m_cardsLayout->addWidget(createCard(provider, expanded), 0);
    }
    if (m_visibleCount == 0) {
        auto *empty = new QLabel(tr("No provider usage data available"), this);
        empty->setContentsMargins(8, 8, 8, 8);
        m_cardsLayout->addWidget(empty);
    }
    updateGeometry();
}

QSize MenuWidget::sizeHint() const {
    const int rows = qMax(1, m_barCount);
    return QSize(360, 28 + rows * 28);
}

QSize MenuWidget::minimumSizeHint() const { return QSize(320, 72); }
