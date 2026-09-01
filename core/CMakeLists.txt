add_library(one_core STATIC
    src/ui_mode.cpp
)
add_library(One::Core ALIAS one_core)

target_include_directories(one_core
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
)

target_compile_features(one_core PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(one_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(one_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

