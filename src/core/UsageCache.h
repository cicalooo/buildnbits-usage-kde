#pragma once

#include "Provider.h"

class UsageCache {
public:
    static QString rootDir();
    static QString cachePath();
    static void save(ProviderID id, const UsageSnapshot &snapshot);
    static bool load(ProviderID id, UsageSnapshot *snapshot);
};
