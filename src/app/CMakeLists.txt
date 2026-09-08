add_executable(buildnbits-usage)

target_sources(buildnbits-usage PRIVATE
    main.cpp
    TrayIcon.cpp
    IconRenderer.cpp
    SettingsDialog.cpp
    MenuWidget.cpp
    UsagePopup.cpp
)

target_link_libraries(buildnbits-usage PRIVATE
    buildnbits-usage-core
    Qt6::Gui
    Qt6::Widgets
    KF6::StatusNotifierItem
    KF6::CoreAddons
    KF6::ConfigCore
    KF6::I18n
)

target_include_directories(buildnbits-usage PRIVATE ${CMAKE_SOURCE_DIR}/src/core)

install(TARGETS buildnbits-usage ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
install(FILES ${CMAKE_SOURCE_DIR}/resources/buildnbits-usage.desktop DESTINATION ${KDE_INSTALL_APPDIR})
install(FILES ${CMAKE_SOURCE_DIR}/resources/kdecodexbar.png DESTINATION ${KDE_INSTALL_ICONDIR}/hicolor/512x512/apps RENAME buildnbits-usage.png)
