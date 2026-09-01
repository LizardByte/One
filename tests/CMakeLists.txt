add_executable(one_ui_mode_test
    core/ui_mode_test.cpp
)

target_link_libraries(one_ui_mode_test PRIVATE One::Core)

add_test(NAME one.ui_mode COMMAND one_ui_mode_test)

add_executable(one_host_model_test
    core/host_model_test.cpp
)

target_link_libraries(one_host_model_test PRIVATE One::Core)

add_test(NAME one.host_model COMMAND one_host_model_test)

add_executable(one_pairing_session_test
    core/pairing_session_test.cpp
)

target_link_libraries(one_pairing_session_test PRIVATE One::Core)

add_test(NAME one.pairing_session COMMAND one_pairing_session_test)

add_executable(one_protocol_test
    protocol/stream_configuration_test.cpp
)

target_link_libraries(one_protocol_test PRIVATE One::Protocol)

add_test(NAME one.protocol COMMAND one_protocol_test)
