add_executable(one_ui_mode_test
    core/ui_mode_test.cpp
)

target_link_libraries(one_ui_mode_test PRIVATE One::Core)

add_test(NAME one.ui_mode COMMAND one_ui_mode_test)

