set(BUILD_SHARED_LIBS OFF)
set(ENET_NO_INSTALL ON)
set(USE_MBEDTLS "${ONE_PROTOCOL_USE_MBEDTLS}")

set(ONE_MOONLIGHT_COMMON_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vendor/moonlight-common-c")
if(NOT EXISTS "${ONE_MOONLIGHT_COMMON_DIR}/CMakeLists.txt" OR
   NOT EXISTS "${ONE_MOONLIGHT_COMMON_DIR}/enet/CMakeLists.txt" OR
   NOT EXISTS "${ONE_MOONLIGHT_COMMON_DIR}/nanors/rs.c")
    message(FATAL_ERROR
        "moonlight-common-c or its dependencies are not initialized. "
        "Run: git submodule update --init --recursive"
    )
endif()

add_subdirectory("${ONE_MOONLIGHT_COMMON_DIR}" moonlight-common-c EXCLUDE_FROM_ALL)

add_library(one_protocol STATIC
    src/stream_configuration.cpp
)
add_library(One::Protocol ALIAS one_protocol)

target_include_directories(one_protocol
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
)

target_compile_features(one_protocol PUBLIC cxx_std_20)
target_link_libraries(one_protocol PRIVATE moonlight-common-c)

if(MSVC)
    target_compile_options(one_protocol PRIVATE /W4 /permissive-)
else()
    target_compile_options(one_protocol PRIVATE -Wall -Wextra -Wpedantic)
endif()
