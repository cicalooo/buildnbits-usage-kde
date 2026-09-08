add_library(buildnbits-usage-core STATIC)

target_sources(buildnbits-usage-core PRIVATE
    Provider.cpp
    ProviderRegistry.cpp
    CodexProvider.cpp
    AntigravityProvider.cpp
    GrokProvider.cpp
    PtySession.cpp
    UsageCache.cpp
)

target_link_libraries(buildnbits-usage-core PUBLIC
    Qt6::Core
    Qt6::Network
    util
)

target_include_directories(buildnbits-usage-core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
