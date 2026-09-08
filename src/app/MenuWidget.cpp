#include "MenuWidget.h"
#include "IconRenderer.h"
#include "Provider.h"

#include <algorithm>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QVBoxLayout>
#include <QDateTime>

MenuWidget::MenuWidget(QWidget *parent) : QWidget(parent), m_cardsLayout(new QVBoxLayout(this)) {
    m_cardsLayout->setContentsMargins(10, 10, 10, 10);
    m_cardsLayout->setSpacing(8);
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Minimum);
}

void MenuWidget::clearCards() {
    while (QLayoutItem *item = m_cardsLayout->takeAt(0)) {
        if (item->widget())
            delete item->widget();
        delete item;
    }
}

static QString ageLabel(const QDateTime &ts) {
    if (!ts.isValid())
        return QString();
    const qint64 secs = ts.secsTo(QDateTime::currentDateTime());
    if (secs < 10)
        return QStringLiteral("now");
    if (secs < 60)
        return QStringLiteral("%1s").arg(secs);
    if (secs < 3600)
        return QStringLiteral("%1m").arg(secs / 60);
    return ts.toString(QStringLiteral("HH:mm"));
}

QWidget *MenuWidget::createCard(Provider *provider) {
    const QColor brand = IconRenderer::brandFor(provider->id());
    auto *card = new QFrame(this);
    card->setFrameShape(QFrame::NoFrame);
    card->setStyleSheet(QStringLiteral(
        "QFrame#squareCard { background: palette(base); border: 1px solid palette(mid); border-radius: 12px; }"
        "QLabel { border: none; background: transparent; }"));
    card->setObjectName(QStringLiteral("squareCard"));

    auto *outer = new QHBoxLayout(card);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    auto *rail = new QFrame(card);
    rail->setFixedWidth(4);
    rail->setStyleSheet(QStringLiteral("background: %1; border: none; border-top-left-radius: 12px; border-bottom-left-radius: 12px;")
                            .arg(brand.name()));
    outer->addWidget(rail);

    auto *layout = new QVBoxLayout();
    layout->setContentsMargins(12, 10, 12, 10);
    layout->setSpacing(6);
    outer->addLayout(layout, 1);

    auto *header = new QHBoxLayout();
    auto *dot = new QLabel(card);
    dot->setFixedSize(8, 8);
    dot->setStyleSheet(QStringLiteral("background: %1; border-radius: 4px;").arg(brand.name()));
    header->addWidget(dot, 0, Qt::AlignVCenter);
    auto *title = new QLabel(provider->name(), card);
    QFont titleFont = title->font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() + 1);
    title->setFont(titleFont);
    header->addWidget(title);
    header->addStretch();

    auto snapshot = provider->snapshot();
    std::sort(snapshot.limits.begin(), snapshot.limits.end(),
              [](const UsageLimit &a, const UsageLimit &b) {
                  return a.displayPercent() < b.displayPercent();
              });
    const QString source = snapshot.source.isEmpty() ? QStringLiteral("cli") : snapshot.source;
    auto *chip = new QLabel(source + QStringLiteral(" · ") + ageLabel(snapshot.timestamp), card);
    chip->setStyleSheet(QStringLiteral("color: palette(mid); font-size: 11px;"));
    header->addWidget(chip);
    layout->addLayout(header);

    const ProviderState state = provider->state();
    if (state == ProviderState::SignedOut) {
        auto *hero = new QLabel(QStringLiteral("—"), card);
        QFont hf = hero->font();
        hf.setBold(true);
        hf.setPointSize(hf.pointSize() + 8);
        hero->setFont(hf);
        hero->setStyleSheet(QStringLiteral("color: %1;").arg(brand.name()));
        layout->addWidget(hero);
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

    if (state == ProviderState::Stale) {
        auto *banner = new QLabel(provider->id() == ProviderID::Antigravity
                                      ? tr("App closed · last snapshot")
                                      : tr("Cached"),
                                  card);
        banner->setStyleSheet(QStringLiteral("color: palette(mid);"));
        layout->addWidget(banner);
    }

    const UsageLimit &heroLimit = snapshot.limits.first();
    auto *heroRow = new QHBoxLayout();
    auto *hero = new QLabel(QStringLiteral("%1%").arg(qRound(heroLimit.displayPercent())), card);
    QFont hf = hero->font();
    hf.setBold(true);
    hf.setPointSize(hf.pointSize() + 8);
    hero->setFont(hf);
    hero->setStyleSheet(QStringLiteral("color: %1;").arg(brand.name()));
    heroRow->addWidget(hero);
    heroRow->addStretch();
    auto *reset = new QLabel(heroLimit.resetDescription.isEmpty() ? tr("reset unknown") : heroLimit.resetDescription, card);
    reset->setStyleSheet(QStringLiteral("color: palette(mid);"));
    heroRow->addWidget(reset, 0, Qt::AlignBottom);
    layout->addLayout(heroRow);

    auto addBar = [&](const UsageLimit &limit, int height) {
        auto *row = new QLabel(QStringLiteral("%1  %2%")
                                   .arg(limit.label)
                                   .arg(qRound(limit.displayPercent())),
                               card);
        layout->addWidget(row);
        auto *bar = new QProgressBar(card);
        bar->setRange(0, 100);
        bar->setValue(qBound(0, qRound(limit.displayPercent()), 100));
        bar->setTextVisible(false);
        bar->setFixedHeight(height);
        bar->setStyleSheet(QStringLiteral(
            "QProgressBar { background: rgba(127,127,127,40); border: none; border-radius: 3px; }"
            "QProgressBar::chunk { background: %1; border-radius: 3px; }")
                               .arg(brand.name()));
        layout->addWidget(bar);
    };

    addBar(heroLimit, 6);
    for (int i = 1; i < snapshot.limits.size() && i < 5; ++i)
        addBar(snapshot.limits.at(i), 4);

    if (provider->id() == ProviderID::Grok) {
        auto *note = new QLabel(tr("Credits % · not a session window"), card);
        note->setStyleSheet(QStringLiteral("color: palette(mid); font-size: 11px;"));
        layout->addWidget(note);
    }

    auto *footer = new QLabel(tr("last good %1").arg(snapshot.timestamp.toString(QStringLiteral("HH:mm"))), card);
    footer->setStyleSheet(QStringLiteral("color: palette(mid); font-size: 11px;"));
    layout->addWidget(footer);
    return card;
}

void MenuWidget::updateData(const QVector<Provider *> &providers) {
    clearCards();
    m_visibleCount = providers.size();
    for (auto *provider : providers)
        m_cardsLayout->addWidget(createCard(provider), 1);
    if (m_visibleCount == 0) {
        auto *empty = new QLabel(tr("No provider usage data available"), this);
        empty->setContentsMargins(10, 8, 10, 8);
        m_cardsLayout->addWidget(empty);
    }
    updateGeometry();
}

QSize MenuWidget::sizeHint() const {
    const int count = qMax(1, m_visibleCount);
    return QSize(340, 160 * count + 20);
}

QSize MenuWidget::minimumSizeHint() const { return QSize(300, 140); }
