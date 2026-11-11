# Load the release asset associated with third-party/build-deps, as Sunshine does.
include_guard(GLOBAL)

if(WIN32 AND NOT MINGW)
    message(FATAL_ERROR "The Windows build-deps package requires the MSYS2 MinGW toolchain.")
endif()

if(NOT DEFINED FFMPEG_PREPARED_BINARIES)
    find_package(Git REQUIRED)
    set(_one_build_deps "${CMAKE_SOURCE_DIR}/third-party/build-deps")
    if(NOT EXISTS "${_one_build_deps}/.git")
        message(FATAL_ERROR "Initialize build-deps with: git submodule update --init third-party/build-deps")
    endif()
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" -C "${_one_build_deps}" describe --tags --exact-match
        OUTPUT_VARIABLE FFMPEG_RELEASE_TAG
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if(NOT FFMPEG_RELEASE_TAG)
        # Shallow submodule checkouts may not contain the release tag yet.
        execute_process(
            COMMAND "${GIT_EXECUTABLE}" -C "${_one_build_deps}" fetch --tags --depth=1
            OUTPUT_QUIET
            ERROR_QUIET
            TIMEOUT 300
        )
        execute_process(
            COMMAND "${GIT_EXECUTABLE}" -C "${_one_build_deps}" describe --tags --exact-match
            OUTPUT_VARIABLE FFMPEG_RELEASE_TAG
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )
    endif()
    if(NOT FFMPEG_RELEASE_TAG)
        message(FATAL_ERROR "The build-deps submodule must be checked out at a published release tag.")
    endif()

    if(NOT DEFINED FFMPEG_RELEASE_ASSET_NAME)
        string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" _one_processor)
        if(_one_processor STREQUAL "amd64")
            set(_one_processor x86_64)
        elseif(_one_processor STREQUAL "arm64")
            set(_one_processor aarch64)
        elseif(_one_processor STREQUAL "powerpc64le")
            set(_one_processor ppc64le)
        endif()
        if(WIN32)
            if(_one_processor STREQUAL "x86_64")
                set(_one_package_platform Windows-AMD64)
            elseif(_one_processor STREQUAL "aarch64")
                set(_one_package_platform Windows-ARM64)
            endif()
        elseif(APPLE)
            if(CMAKE_OSX_ARCHITECTURES)
                set(_one_processor "${CMAKE_OSX_ARCHITECTURES}")
            endif()
            if(_one_processor STREQUAL "aarch64" OR _one_processor STREQUAL "arm64")
                set(_one_package_platform Darwin-arm64)
            elseif(_one_processor STREQUAL "x86_64")
                set(_one_package_platform Darwin-x86_64)
            endif()
        elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
            if(CMAKE_C_COMPILER_TARGET MATCHES "musl" OR EXISTS "/etc/alpine-release")
                set(_one_package_platform "Alpine-${_one_processor}")
            else()
                set(_one_package_platform "Linux-${_one_processor}")
            endif()
        elseif(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD" AND _one_processor STREQUAL "x86_64")
            set(_one_package_platform FreeBSD-amd64)
        endif()
        if(NOT _one_package_platform)
            message(FATAL_ERROR "No build-deps FFmpeg package for ${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}. "
                    "Set FFMPEG_RELEASE_ASSET_NAME or FFMPEG_PREPARED_BINARIES for this target.")
        endif()
        set(FFMPEG_RELEASE_ASSET_NAME "${_one_package_platform}-ffmpeg.tar.gz")
    endif()

    set(FFMPEG_DOWNLOAD_DIR "${CMAKE_BINARY_DIR}/_deps")
    set(FFMPEG_VERSION_DIR "${FFMPEG_DOWNLOAD_DIR}/ffmpeg-${FFMPEG_RELEASE_TAG}")
    set(FFMPEG_PREPARED_BINARIES "${FFMPEG_VERSION_DIR}/ffmpeg")
    set(FFMPEG_ARCHIVE_PATH "${FFMPEG_VERSION_DIR}/${FFMPEG_RELEASE_ASSET_NAME}")
    set(FFMPEG_RELEASE_URL "https://github.com/LizardByte/build-deps/releases/download/${FFMPEG_RELEASE_TAG}")
    message(STATUS "Using FFmpeg from build-deps tag: ${FFMPEG_RELEASE_TAG}")

    if(NOT EXISTS "${FFMPEG_PREPARED_BINARIES}/lib/libavcodec.a")
        file(MAKE_DIRECTORY "${FFMPEG_VERSION_DIR}")
        if(NOT EXISTS "${FFMPEG_ARCHIVE_PATH}")
            message(STATUS "Downloading FFmpeg binaries from ${FFMPEG_RELEASE_URL}/${FFMPEG_RELEASE_ASSET_NAME}")
            file(DOWNLOAD
                "${FFMPEG_RELEASE_URL}/${FFMPEG_RELEASE_ASSET_NAME}"
                "${FFMPEG_ARCHIVE_PATH}.part"
                SHOW_PROGRESS
                STATUS FFMPEG_DOWNLOAD_STATUS
                TLS_VERIFY ON
                TIMEOUT 300
            )
            list(GET FFMPEG_DOWNLOAD_STATUS 0 FFMPEG_DOWNLOAD_STATUS_CODE)
            list(GET FFMPEG_DOWNLOAD_STATUS 1 FFMPEG_DOWNLOAD_STATUS_MESSAGE)
            if(NOT FFMPEG_DOWNLOAD_STATUS_CODE EQUAL 0)
                message(FATAL_ERROR "Failed to download FFmpeg binaries: ${FFMPEG_DOWNLOAD_STATUS_MESSAGE}")
            endif()
            file(RENAME "${FFMPEG_ARCHIVE_PATH}.part" "${FFMPEG_ARCHIVE_PATH}")
        endif()
        file(ARCHIVE_EXTRACT  # cmake-lint: disable=E1126
            INPUT "${FFMPEG_ARCHIVE_PATH}"
            DESTINATION "${FFMPEG_VERSION_DIR}"
        )
    else()
        message(STATUS "Using existing FFmpeg binaries at ${FFMPEG_PREPARED_BINARIES}")
    endif()
else()
    message(STATUS "Using user-specified FFmpeg binaries at ${FFMPEG_PREPARED_BINARIES}")
endif()

if(NOT EXISTS "${FFMPEG_PREPARED_BINARIES}/lib/libavcodec.a"
        OR NOT EXISTS "${FFMPEG_PREPARED_BINARIES}/lib/libdav1d.a"
        OR NOT EXISTS "${FFMPEG_PREPARED_BINARIES}/lib/libopus.a")
    message(FATAL_ERROR "FFMPEG_PREPARED_BINARIES must contain the build-deps FFmpeg package "
            "with FFmpeg, dav1d, and Opus.")
endif()

# Refresh cached paths when the package or system library search policy changes.
get_cmake_property(_one_cache_variables CACHE_VARIABLES)
foreach(variable IN LISTS _one_cache_variables)
    if(variable MATCHES "^(pkgcfg_lib_|__pkg_config_checked_)?ONE_BUNDLED_(FFMPEG|OPUS)")
        unset(${variable} CACHE)
    endif()
endforeach()
foreach(variable PKG_CONFIG_PATH PKG_CONFIG_LIBDIR PKG_CONFIG_SYSROOT_DIR)
    if(DEFINED ENV{${variable}})
        set(_one_saved_${variable} "$ENV{${variable}}")
    else()
        unset(_one_saved_${variable})
    endif()
endforeach()
set(_one_saved_pkg_config_argn "${PKG_CONFIG_ARGN}")
# The package supplies static codec archives; system libraries keep their normal linkage.
set(ENV{PKG_CONFIG_PATH} "${FFMPEG_PREPARED_BINARIES}/lib/pkgconfig")
set(ENV{PKG_CONFIG_LIBDIR} "$ENV{PKG_CONFIG_PATH}")
unset(ENV{PKG_CONFIG_SYSROOT_DIR})
set(PKG_CONFIG_ARGN --static)
if(WIN32)
    set(_one_saved_library_suffixes "${CMAKE_FIND_LIBRARY_SUFFIXES}")
    set(CMAKE_FIND_LIBRARY_SUFFIXES .a)
endif()
pkg_check_modules(ONE_BUNDLED_FFMPEG REQUIRED IMPORTED_TARGET libavcodec libavutil libswscale)
pkg_check_modules(ONE_BUNDLED_OPUS REQUIRED IMPORTED_TARGET opus)
foreach(variable PKG_CONFIG_PATH PKG_CONFIG_LIBDIR PKG_CONFIG_SYSROOT_DIR)
    if(DEFINED _one_saved_${variable})
        set(ENV{${variable}} "${_one_saved_${variable}}")
    else()
        unset(ENV{${variable}})
    endif()
endforeach()
set(PKG_CONFIG_ARGN "${_one_saved_pkg_config_argn}")
if(WIN32)
    set(CMAKE_FIND_LIBRARY_SUFFIXES "${_one_saved_library_suffixes}")
endif()

add_library(one_ffmpeg INTERFACE)
add_library(One::FFmpeg ALIAS one_ffmpeg)
target_link_libraries(one_ffmpeg INTERFACE PkgConfig::ONE_BUNDLED_FFMPEG PkgConfig::ONE_BUNDLED_OPUS)
set(ONE_FFMPEG_FOUND TRUE)
set(ONE_OPUS_FOUND TRUE)
