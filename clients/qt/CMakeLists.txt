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
        pkg_check_modules(ONE_FFMPEG QUIET libavcodec libavutil libswscale)
        pkg_check_modules(ONE_OPUS QUIET opus)
    endif()
    if(ONE_FFMPEG_FOUND AND ONE_OPUS_FOUND AND TARGET SDL3::SDL3-static)
        set(ONE_HAS_DESKTOP_MEDIA ON)
        add_library(one_desktop_media_dependencies INTERFACE)
        target_include_directories(one_desktop_media_dependencies INTERFACE
            ${ONE_FFMPEG_INCLUDE_DIRS}
            ${ONE_OPUS_INCLUDE_DIRS}
        )
        target_link_directories(one_desktop_media_dependencies INTERFACE
            ${ONE_FFMPEG_STATIC_LIBRARY_DIRS}
            ${ONE_OPUS_STATIC_LIBRARY_DIRS}
        )
        if(MINGW AND ONE_PREFER_STATIC_DEPENDENCIES)
            # The complete MSYS2 FFmpeg static closure contains conflicting Rust
            # runtimes and a Windows-incompatible libdl dependency. Keep the
            # small FFmpeg ABI surface shared until One has a decoder-only build;
            # Opus and SDL3 remain static here.
            target_link_libraries(one_desktop_media_dependencies INTERFACE
                "-Wl,-Bdynamic"
                ${ONE_FFMPEG_LIBRARIES}
                "-Wl,-Bstatic"
                ${ONE_OPUS_STATIC_LIBRARIES}
                "-Wl,-Bdynamic"
                SDL3::SDL3-static
            )
        else()
            target_link_libraries(one_desktop_media_dependencies INTERFACE
                ${ONE_FFMPEG_LINK_LIBRARIES}
                ${ONE_OPUS_LINK_LIBRARIES}
                SDL3::SDL3-static
            )
        endif()
        target_link_options(one_desktop_media_dependencies INTERFACE
            ${ONE_FFMPEG_LDFLAGS_OTHER}
            ${ONE_OPUS_STATIC_LDFLAGS_OTHER}
        )
        message(STATUS "One desktop media backend enabled with decoder-enabled FFmpeg")
    else()
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
        qml/StreamPage.qml
    SOURCES
        src/stream_video_output.cpp
        src/stream_video_output.hpp
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
        set_tests_properties(one.qt.media_backend PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

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
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        )
    endforeach()
endif()
