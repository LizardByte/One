# Resolve static archives before any dependency creates an imported target.
if(MINGW)
    set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
    set(CMAKE_IMPORT_LIBRARY_SUFFIX ".a")
    set(PKG_CONFIG_ARGN --static)
    add_link_options(-static)

    get_filename_component(_one_compiler_bin "${CMAKE_CXX_COMPILER}" DIRECTORY)
    get_filename_component(_one_toolchain_prefix "${_one_compiler_bin}" DIRECTORY)
    if(NOT Qt6_DIR AND EXISTS "${_one_toolchain_prefix}/qt6-static/lib/cmake/Qt6/Qt6Config.cmake")
        list(PREPEND CMAKE_PREFIX_PATH "${_one_toolchain_prefix}/qt6-static")
    endif()

    # Match the MSYS2 static Qt dependency setup used in Sunshine: HarfBuzz's
    # config target and FindTIFF omit parts of their static link interfaces.
    set(CMAKE_DISABLE_FIND_PACKAGE_harfbuzz TRUE)
    if(ONE_BUILD_QT_CLIENT)
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(ONE_STATIC_TIFF REQUIRED IMPORTED_TARGET libtiff-4)
        add_library(TIFF::TIFF INTERFACE IMPORTED)
        target_link_libraries(TIFF::TIFF INTERFACE PkgConfig::ONE_STATIC_TIFF)
    endif()
endif()
