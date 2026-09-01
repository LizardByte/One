set(BUILD_DOC OFF CACHE BOOL "Do not build qmdnsengine documentation" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "Do not build qmdnsengine examples" FORCE)
set(BUILD_TESTS OFF CACHE BOOL "Do not build qmdnsengine's tests" FORCE)
add_subdirectory(vendor/qmdnsengine qmdnsengine EXCLUDE_FROM_ALL)
find_package(OpenSSL 1.1 REQUIRED COMPONENTS Crypto)

add_library(one_qt_client STATIC
    src/application.cpp
    src/application.hpp
    src/application_list_client.cpp
    src/application_list_client.hpp
    src/application_list_model.cpp
    src/application_list_model.hpp
    src/client_identity.cpp
    src/client_identity.hpp
    src/host_discovery_service.cpp
    src/host_discovery_service.hpp
    src/host_list_model.cpp
    src/host_list_model.hpp
    src/host_settings.cpp
    src/host_settings.hpp
    src/launch_request.cpp
    src/launch_request.hpp
    src/pairing_client.cpp
    src/pairing_client.hpp
    src/server_info.cpp
    src/server_info.hpp
    src/server_info_client.cpp
    src/server_info_client.hpp
)
add_library(One::QtClient ALIAS one_qt_client)

target_include_directories(one_qt_client PUBLIC src)
target_link_libraries(one_qt_client
    PUBLIC One::Core One::Protocol Qt6::Concurrent Qt6::Core Qt6::Network
    PRIVATE OpenSSL::Crypto qmdnsengine
)
if(WIN32 AND ONE_PREFER_STATIC_DEPENDENCIES)
    target_link_libraries(one_qt_client PRIVATE ws2_32 gdi32 crypt32)
endif()

qt_add_executable(one
    src/main.cpp
    src/ui_environment.cpp
    src/ui_environment.hpp
)

target_compile_definitions(one PRIVATE ONE_VERSION="${PROJECT_VERSION}")

target_link_libraries(one
    PRIVATE
        One::Core
        One::Protocol
        One::QtClient
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
        qml/GamesPage.qml
        qml/SettingsPage.qml
)

set_target_properties(one PROPERTIES
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

if(ONE_BUILD_TESTS)
    add_executable(one_qt_host_list_model_test
        tests/host_list_model_test.cpp
    )
    target_link_libraries(one_qt_host_list_model_test PRIVATE One::QtClient)
    add_test(NAME one.qt.host_list_model COMMAND one_qt_host_list_model_test)

    add_executable(one_server_info_test
        tests/server_info_test.cpp
    )
    target_link_libraries(one_server_info_test PRIVATE One::QtClient)
    add_test(NAME one.qt.server_info COMMAND one_server_info_test)

    add_executable(one_pairing_client_test
        tests/pairing_client_test.cpp
    )
    target_link_libraries(one_pairing_client_test PRIVATE One::QtClient)
    add_test(NAME one.qt.pairing_client COMMAND one_pairing_client_test)

    add_executable(one_application_list_test
        tests/application_list_test.cpp
    )
    target_link_libraries(one_application_list_test PRIVATE One::QtClient)
    add_test(NAME one.qt.application_list COMMAND one_application_list_test)

    add_executable(one_launch_request_test
        tests/launch_request_test.cpp
    )
    target_link_libraries(one_launch_request_test PRIVATE One::QtClient)
    add_test(NAME one.qt.launch_request COMMAND one_launch_request_test)

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
