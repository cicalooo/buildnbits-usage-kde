#include "IconRenderer.h"

#include <QFont>
#include <QGuiApplication>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QScreen>

static QSize logicalIconSize() { return {22, 22}; }

QColor IconRenderer::brandFor(ProviderID id) {
    switch (id) {
    case ProviderID::Codex:
        return QColor(QStringLiteral("#22C55E"));
    case ProviderID::Grok:
        return QColor(QStringLiteral("#F97316"));
    case ProviderID::Antigravity:
        return QColor(QStringLiteral("#3B82F6"));
    default:
        return QColor(Qt::white);
    }
}

QIcon IconRenderer::renderSquare(const SquarePaint &paint, qreal dpr) {
    if (dpr < 1.0)
        dpr = 1.0;
    const QSize logical = logicalIconSize();
    QPixmap pixmap(logical * dpr);
    pixmap.setDevicePixelRatio(dpr);
    pixmap.fill(Qt::transparent);

    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);

    const QRectF box(1.5, 1.5, logical.width() - 3.0, logical.height() - 3.0);
    QColor brand = paint.brand;
    const int heat = paint.usedForHeat >= 0 ? paint.usedForHeat : paint.value;
    const bool heat70 = !paint.signedOut && heat >= 70 && heat < 90;
    const bool heat90 = !paint.signedOut && heat >= 90;

    qreal fillAlpha = paint.dark ? 0.32 : 0.18;
    if (heat70 || heat90)
        fillAlpha = 0.50;

    QColor fill = brand;
    fill.setAlphaF(fillAlpha);
    QColor border = brand;
    QColor text = paint.dark ? brand.lighter(160) : brand.darker(180);

    if (paint.stale) {
        fill.setAlphaF(fill.alphaF() * 0.5);
        border.setAlphaF(0.5);
        text.setAlphaF(0.7);
    }

    if (paint.signedOut || paint.error) {
        p.setBrush(Qt::NoBrush);
        QPen pen(brand, 1.0, Qt::DashLine);
        p.setPen(pen);
        p.drawRoundedRect(box, 4, 4);
        p.setPen(text);
        QFont font;
        font.setBold(true);
        font.setPixelSize(10);
        p.setFont(font);
        p.drawText(box, Qt::AlignCenter, QStringLiteral("—"));
        p.end();
        QIcon icon;
        icon.addPixmap(pixmap);
        return icon;
    }

    p.setPen(QPen(border, heat90 ? 1.6 : 1.0));
    p.setBrush(fill);
    p.drawRoundedRect(box, 4, 4);

    if (heat70 || heat90) {
        QRectF inner = box.adjusted(2, 2, -2, -2);
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(border, 1.2));
        p.drawRoundedRect(inner, 3, 3);
    }

    p.setPen(text);
    QFont font;
    font.setBold(true);
    font.setPixelSize(paint.value >= 100 ? 8 : 10);
    p.setFont(font);
    const QString label = paint.value < 0 ? QStringLiteral("—") : QString::number(paint.value);
    p.drawText(box, Qt::AlignCenter, label);

    if (paint.stale) {
        p.setPen(Qt::NoPen);
        p.setBrush(border);
        p.drawEllipse(QPointF(box.center().x(), box.bottom() - 2.5), 1.2, 1.2);
    }

    p.end();
    QIcon icon;
    icon.addPixmap(pixmap);
    return icon;
}

QIcon IconRenderer::renderPlaceholder() {
    SquarePaint paint;
    paint.brand = QColor(Qt::white);
    paint.signedOut = true;
    paint.dark = true;
    qreal dpr = 1.0;
    if (QGuiApplication::primaryScreen())
        dpr = QGuiApplication::primaryScreen()->devicePixelRatio();
    return renderSquare(paint, dpr);
}

QIcon IconRenderer::renderIcon(const UsageSnapshot &snapshot, bool isDarkTheme) {
    SquarePaint paint;
    paint.brand = QColor(Qt::white);
    paint.dark = isDarkTheme;
    if (!snapshot.limits.isEmpty())
        paint.value = qBound(0, qRound(snapshot.limits.first().displayPercent()), 100);
    qreal dpr = 1.0;
    if (QGuiApplication::primaryScreen())
        dpr = QGuiApplication::primaryScreen()->devicePixelRatio();
    return renderSquare(paint, dpr);
}
