#pragma once

#include <QColor>
#include <QIcon>
#include <QSize>
#include "Provider.h"

struct SquarePaint {
    QColor brand;
    int value = -1; // -1 draws an em dash
    int usedForHeat = -1; // used %, independent of remaining display
    bool stale = false;
    bool signedOut = false;
    bool error = false;
    bool dark = true;
};

class IconRenderer {
public:
    static QColor brandFor(ProviderID id);
    static QIcon renderSquare(const SquarePaint &paint, qreal dpr = 1.0);
    static QIcon renderPlaceholder();
    static QIcon renderIcon(const UsageSnapshot &snapshot, bool isDarkTheme = false);
};
