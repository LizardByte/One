# A small, decoder-only build. Sunshine's FFmpeg package is encoder-focused,
# and the general MSYS2 static archive pulls in unrelated encoder runtimes.
if(NOT MINGW)
    message(FATAL_ERROR "The bundled static Windows FFmpeg build currently requires the MSYS2 MinGW toolchain.")
endif()
include(ExternalProject)
find_program(ONE_MSYS_BASH bash HINTS "${_one_toolchain_prefix}/../usr/bin" NO_DEFAULT_PATH REQUIRED)
pkg_check_modules(ONE_DAV1D REQUIRED IMPORTED_TARGET dav1d)
set(ONE_FFMPEG_JOBS 8 CACHE STRING "Parallel jobs for the decoder-only FFmpeg build")
set(_one_ffmpeg_prefix "${CMAKE_BINARY_DIR}/deps/ffmpeg")
set(_one_ffmpeg_script "${PROJECT_SOURCE_DIR}/scripts/build-ffmpeg.sh")
ExternalProject_Add(one_ffmpeg_build
    URL https://ffmpeg.org/releases/ffmpeg-9.0.1.tar.xz
    URL_HASH SHA256=cf38e0e28c7e5605942c4a77755349b0145804a397af37eb1fb4c77cb237f635
    DOWNLOAD_DIR "${PROJECT_SOURCE_DIR}/build/deps/downloads"
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    PREFIX "${CMAKE_BINARY_DIR}/deps/ffmpeg-project"
    CONFIGURE_COMMAND "${ONE_MSYS_BASH}" "${_one_ffmpeg_script}" configure
        <SOURCE_DIR> "${_one_ffmpeg_prefix}" "${CMAKE_C_COMPILER}" "${ONE_FFMPEG_JOBS}"
    BUILD_COMMAND "${ONE_MSYS_BASH}" "${_one_ffmpeg_script}" build
        <SOURCE_DIR> "${_one_ffmpeg_prefix}" "${CMAKE_C_COMPILER}" "${ONE_FFMPEG_JOBS}"
    INSTALL_COMMAND "${ONE_MSYS_BASH}" "${_one_ffmpeg_script}" install
        <SOURCE_DIR> "${_one_ffmpeg_prefix}" "${CMAKE_C_COMPILER}" "${ONE_FFMPEG_JOBS}"
    BUILD_BYPRODUCTS
        "${_one_ffmpeg_prefix}/lib/libavcodec.a"
        "${_one_ffmpeg_prefix}/lib/libavutil.a"
        "${_one_ffmpeg_prefix}/lib/libswscale.a"
)
ExternalProject_Add_StepDependencies(one_ffmpeg_build configure "${_one_ffmpeg_script}")
file(MAKE_DIRECTORY "${_one_ffmpeg_prefix}/include")
foreach(_one_component IN ITEMS avcodec avutil swscale)
    add_library(one_ffmpeg_${_one_component} STATIC IMPORTED)
    set_target_properties(one_ffmpeg_${_one_component} PROPERTIES
        IMPORTED_LOCATION "${_one_ffmpeg_prefix}/lib/lib${_one_component}.a"
        INTERFACE_INCLUDE_DIRECTORIES "${_one_ffmpeg_prefix}/include"
    )
    add_dependencies(one_ffmpeg_${_one_component} one_ffmpeg_build)
endforeach()
target_link_libraries(one_ffmpeg_avcodec INTERFACE one_ffmpeg_avutil PkgConfig::ONE_DAV1D)
target_link_libraries(one_ffmpeg_swscale INTERFACE one_ffmpeg_avutil)
target_link_libraries(one_ffmpeg_avutil INTERFACE bcrypt)
add_library(one_ffmpeg INTERFACE)
add_library(One::FFmpeg ALIAS one_ffmpeg)
target_link_libraries(one_ffmpeg INTERFACE one_ffmpeg_avcodec one_ffmpeg_swscale)
set(ONE_FFMPEG_FOUND TRUE)
