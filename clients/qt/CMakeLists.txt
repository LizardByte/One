# MSYS2 upgrades preserve package file timestamps. Put Qt's version in every
# compile command so an upgrade cannot leave mixed old/new inline container
# implementations in the executable (the cause of the pairing-worker crash).
add_compile_definitions(ONE_QT_BUILD_VERSION="${Qt6_VERSION}")

if(WIN32 AND ONE_WINDOWS_STATIC)
    get_target_property(_one_qt_type Qt6::Core TYPE)
    if(NOT _one_qt_type STREQUAL "STATIC_LIBRARY")
        message(FATAL_ERROR "ONE_WINDOWS_STATIC requires static Qt. Use a fresh build directory and set CMAKE_PREFIX_PATH to the qt6-static installation.")
    endif()
endif()

set(BUILD_DOC OFF CACHE BOOL "Do not build qmdnsengine documentation" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "Do not build qmdnsengine examples" FORCE)
set(BUILD_TESTS OFF CACHE BOOL "Do not build qmdnsengine's tests" FORCE)
add_subdirectory(vendor/qmdnsengine qmdnsengine EXCLUDE_FROM_ALL)
find_package(OpenSSL 1.1 REQUIRED COMPONENTS Crypto)

set(ONE_HAS_DESKTOP_MEDIA OFF)
if(NOT ANDROID AND NOT CMAKE_SYSTEM_NAME STREQUAL "iOS" AND NOT CMAKE_SYSTEM_NAME STREQUAL "tvOS")
    find_package(PkgConfig QUIET)
    find_package(SDL3 CONFIG QUIET)
    if(PkgConfig_FOUND)
        include(${PROJECT_SOURCE_DIR}/cmake/ffmpeg.cmake)
    endif()
    if(ONE_FFMPEG_FOUND AND ONE_OPUS_FOUND AND TARGET SDL3::SDL3-static)
        set(ONE_HAS_DESKTOP_MEDIA ON)
        add_library(one_desktop_media_dependencies INTERFACE)
        target_link_libraries(one_desktop_media_dependencies INTERFACE One::FFmpeg SDL3::SDL3-static)
        message(STATUS "One desktop media backend enabled with decoder-enabled FFmpeg")
    else()
        if(WIN32 AND ONE_WINDOWS_STATIC)
            message(FATAL_ERROR "The static Windows client requires Opus, static SDL3, and decoder-enabled static FFmpeg.")
        endif()
        message(STATUS "One desktop media backend disabled: decoder-enabled FFmpeg, Opus, and static SDL3 are required")
    endif()
endif()

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
    src/desktop_gamepad_manager.cpp
    src/desktop_gamepad_manager.hpp
    src/main.cpp
    src/opus_audio_renderer.cpp
    src/opus_audio_renderer.hpp
    src/qt_input_mapper.cpp
    src/qt_input_mapper.hpp
    src/streaming_controller.cpp
    src/streaming_controller.hpp
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

if(ONE_HAS_DESKTOP_MEDIA)
    target_compile_definitions(one PRIVATE ONE_HAS_FFMPEG ONE_HAS_PC_MEDIA SDL_MAIN_HANDLED)
    target_link_libraries(one PRIVATE one_desktop_media_dependencies)
endif()

if(MINGW AND ONE_PREFER_STATIC_DEPENDENCIES)
    find_file(
        ONE_WINPTHREAD_STATIC_LIBRARY
        NAMES libwinpthread.a
        HINTS ${CMAKE_CXX_IMPLICIT_LINK_DIRECTORIES}
        REQUIRED
    )
    target_link_options(one PRIVATE
        "-Wl,--whole-archive"
        ${ONE_WINPTHREAD_STATIC_LIBRARY}
        "-Wl,--no-whole-archive"
    )
endif()

set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

set(ONE_QML_FILES
        qml/Main.qml
        qml/AppNavigation.qml
        qml/AboutPage.qml
        qml/ComputersPage.qml
        qml/GamesPage.qml
        qml/SettingsPage.qml
        qml/StreamPage.qml
        qml/Theme.qml
        qml/OneIcon.qml
        qml/OneButton.qml
        qml/Surface.qml
        qml/StatusBadge.qml
        qml/OneDialog.qml
        qml/HostArtwork.qml
        qml/HostAvatar.qml
        qml/HostBackdrop.qml
        qml/PosterArtwork.qml
)

qt_add_qml_module(one
    URI One
    VERSION 1.0
    QML_FILES ${ONE_QML_FILES}
    SOURCES
        src/stream_video_output.cpp
        src/stream_video_output.hpp
)

set_target_properties(one PROPERTIES
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

if(WIN32 AND ONE_WINDOWS_STATIC AND MINGW)
    add_custom_command(TARGET one POST_BUILD
        COMMAND ${CMAKE_COMMAND} -DONE_EXECUTABLE=$<TARGET_FILE:one> -DONE_OBJDUMP=${CMAKE_OBJDUMP}
            -P ${PROJECT_SOURCE_DIR}/cmake/check_windows_linkage.cmake
        VERBATIM
    )
    if(ONE_BUILD_TESTS)
        add_test(NAME one.windows.static_linkage
            COMMAND ${CMAKE_COMMAND} -DONE_EXECUTABLE=$<TARGET_FILE:one> -DONE_OBJDUMP=${CMAKE_OBJDUMP}
                -P ${PROJECT_SOURCE_DIR}/cmake/check_windows_linkage.cmake
        )
    endif()
endif()

if(ONE_BUILD_TESTS)
    find_package(Qt6 6.8 REQUIRED COMPONENTS Test)
    if(WIN32 AND ONE_WINDOWS_STATIC)
        # Static Qt only imports the native platform plugin by default. The
        # headless validation paths must also embed their offscreen backend.
        qt_import_plugins(one INCLUDE Qt6::QOffscreenIntegrationPlugin)
    endif()
    qt_add_executable(one_ui_test
        tests/ui_test.cpp
        src/qt_input_mapper.cpp
    )
    target_include_directories(one_ui_test PRIVATE src)
    target_link_libraries(one_ui_test PRIVATE
        One::Protocol Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2 Qt6::Test
    )
    if(WIN32 AND ONE_WINDOWS_STATIC)
        qt_import_plugins(one_ui_test INCLUDE Qt6::QOffscreenIntegrationPlugin)
    endif()
    qt_add_qml_module(one_ui_test
        URI One
        VERSION 1.0
        OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/ui-test/One
        QML_FILES ${ONE_QML_FILES}
        SOURCES src/stream_video_output.cpp src/stream_video_output.hpp
    )
    qt_add_resources(one_ui_test ui_test_fixtures
        PREFIX "/"
        FILES tests/ui_fixture.qml
    )
    add_test(NAME one.qt.ui_interactions COMMAND one_ui_test)
    set_tests_properties(one.qt.ui_interactions PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_FORCE_STDERR_LOGGING=1;QT_ASSUME_STDERR_HAS_CONSOLE=1"
    )

    add_executable(one_qt_host_list_model_test
        tests/host_list_model_test.cpp
    )
    target_link_libraries(one_qt_host_list_model_test PRIVATE One::QtClient)
    add_test(NAME one.qt.host_list_model COMMAND one_qt_host_list_model_test)

    add_executable(one_authenticated_client_test tests/authenticated_client_test.cpp)
    target_link_libraries(one_authenticated_client_test PRIVATE One::QtClient Qt6::Test)
    add_test(NAME one.qt.authenticated_client COMMAND one_authenticated_client_test)

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

    if(ONE_HAS_DESKTOP_MEDIA)
        add_executable(one_media_backend_test
            src/opus_audio_renderer.cpp
            src/opus_audio_renderer.hpp
            src/qt_input_mapper.cpp
            src/qt_input_mapper.hpp
            src/stream_video_output.cpp
            src/stream_video_output.hpp
            tests/media_backend_test.cpp
        )
        target_include_directories(one_media_backend_test PRIVATE src)
        target_compile_definitions(one_media_backend_test PRIVATE ONE_HAS_FFMPEG ONE_HAS_PC_MEDIA SDL_MAIN_HANDLED)
        target_link_libraries(one_media_backend_test PRIVATE
            One::Protocol
            Qt6::Gui
            Qt6::Qml
            Qt6::Quick
            one_desktop_media_dependencies
        )
        add_test(NAME one.qt.media_backend COMMAND one_media_backend_test)
        if(WIN32 AND ONE_WINDOWS_STATIC)
            qt_import_plugins(one_media_backend_test INCLUDE Qt6::QOffscreenIntegrationPlugin)
        endif()
        set_tests_properties(one.qt.media_backend PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_FORCE_STDERR_LOGGING=1;QT_ASSUME_STDERR_HAS_CONSOLE=1")

        add_executable(one_desktop_input_test
            src/qt_input_mapper.cpp
            src/qt_input_mapper.hpp
            tests/desktop_input_test.cpp
        )
        target_include_directories(one_desktop_input_test PRIVATE src)
        target_link_libraries(one_desktop_input_test PRIVATE One::Protocol Qt6::Core)
        add_test(NAME one.qt.desktop_input COMMAND one_desktop_input_test)

        add_executable(one_desktop_gamepad_test
            src/desktop_gamepad_manager.cpp
            src/desktop_gamepad_manager.hpp
            tests/desktop_gamepad_test.cpp
        )
        target_include_directories(one_desktop_gamepad_test PRIVATE src)
        target_compile_definitions(one_desktop_gamepad_test PRIVATE ONE_HAS_PC_MEDIA SDL_MAIN_HANDLED)
        target_link_libraries(one_desktop_gamepad_test PRIVATE
            One::Protocol
            Qt6::Core
            one_desktop_media_dependencies
        )
        add_test(NAME one.qt.desktop_gamepad COMMAND one_desktop_gamepad_test)
    endif()

    foreach(ui_mode IN ITEMS desktop touch television)
        add_test(
            NAME one.qt.${ui_mode}
            COMMAND one --validate-ui --ui-mode=${ui_mode}
        )
        set_tests_properties(one.qt.${ui_mode} PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_FORCE_STDERR_LOGGING=1;QT_ASSUME_STDERR_HAS_CONSOLE=1"
        )
    endforeach()
endif()
