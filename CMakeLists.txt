cmake_minimum_required(VERSION 3.24)

project(One
        VERSION 0.1.0
        DESCRIPTION "Universal game-streaming client for Sunshine"
        HOMEPAGE_URL "https://github.com/LizardByte/One")

set(PROJECT_LICENSE "GPL-3.0-only")
set(PROJECT_FQDN "dev.lizardbyte.app.One")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ONE_BUILD_QT_CLIENT "Build the shared Qt Quick client" ON)
option(ONE_BUILD_TESTS "Build One's tests" ON)
option(ONE_PREFER_STATIC_DEPENDENCIES "Prefer static linkage for dependencies that support it" ON)
option(ONE_WINDOWS_STATIC "Build Windows executables without third-party DLL dependencies" ${WIN32})
option(ONE_PROTOCOL_USE_MBEDTLS "Use MbedTLS instead of OpenSSL for the streaming protocol" OFF)

if(WIN32 AND ONE_WINDOWS_STATIC)
    set(ONE_PREFER_STATIC_DEPENDENCIES ON)
    include(cmake/windows_static.cmake)
endif()

if(ONE_PREFER_STATIC_DEPENDENCIES)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build dependencies as static libraries" FORCE)
    set(OPENSSL_USE_STATIC_LIBS TRUE)
    if(MINGW)
        add_link_options(-static-libgcc -static-libstdc++)
    endif()
    if(MSVC)
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif()
endif()

if(ONE_BUILD_TESTS)
    enable_testing()
endif()

add_subdirectory(core)

if(ONE_BUILD_QT_CLIENT)
    find_package(Qt6 6.8 REQUIRED COMPONENTS Concurrent Core Gui Network Qml Quick QuickControls2)
    qt_standard_project_setup(REQUIRES 6.8)
    add_subdirectory(clients/qt)
endif()

if(ONE_BUILD_TESTS)
    add_subdirectory(tests)
endif()
