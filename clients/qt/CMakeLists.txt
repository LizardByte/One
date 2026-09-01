qt_add_executable(one
    src/main.cpp
    src/ui_environment.cpp
    src/ui_environment.hpp
)

target_compile_definitions(one PRIVATE ONE_VERSION="${PROJECT_VERSION}")

target_link_libraries(one
    PRIVATE
        One::Core
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
)

qt_add_qml_module(one
    URI One
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/AppNavigation.qml
        qml/AboutPage.qml
        qml/ComputersPage.qml
        qml/SettingsPage.qml
)

set_target_properties(one PROPERTIES
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

if(ONE_BUILD_TESTS)
    foreach(ui_mode IN ITEMS desktop touch television)
        add_test(
            NAME one.qt.${ui_mode}
            COMMAND one --validate-ui --ui-mode=${ui_mode}
        )
        set_tests_properties(one.qt.${ui_mode} PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        )
    endforeach()
endif()
