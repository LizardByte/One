cmake_minimum_required(VERSION 3.24)
execute_process(COMMAND "${ONE_OBJDUMP}" -p "${ONE_EXECUTABLE}"
    RESULT_VARIABLE _one_result OUTPUT_VARIABLE _one_pe ERROR_VARIABLE _one_error)
if(NOT _one_result EQUAL 0)
    message(FATAL_ERROR "Unable to inspect Windows imports: ${_one_error}")
endif()
string(REGEX MATCHALL "DLL Name: [^\r\n]+" _one_imports "${_one_pe}")
if(NOT _one_imports)
    message(FATAL_ERROR "No PE import table found in ${ONE_EXECUTABLE}")
endif()
# Windows API DLLs are supplied by the OS, not shipped with One. Do not admit
# third-party DLLs merely because they happen to be installed on this machine.
set(_one_windows_dlls
    advapi32 authz avrt bcrypt cfgmgr32 comctl32 comdlg32 crypt32 d3d9 d3d11 d3d12
    dinput8 dnsapi dwmapi dwrite dxgi gdi32 hid imm32 iphlpapi kernel32 mf mfplat
    mfuuid ncrypt netapi32 ntdll ole32 oleaut32 powrprof propsys rpcrt4 secur32
    setupapi shcore shell32 shlwapi user32 userenv usp10 uxtheme version winhttp
    winmm winspool wintrust ws2_32 wtsapi32
)
foreach(_one_import IN LISTS _one_imports)
    string(REPLACE "DLL Name: " "" _one_dll "${_one_import}")
    string(STRIP "${_one_dll}" _one_dll)
    string(TOLOWER "${_one_dll}" _one_dll)
    if(_one_dll MATCHES "^(api|ext)-ms-win-.*\\.dll$")
        continue()
    endif()
    string(REGEX REPLACE "\\.dll$" "" _one_name "${_one_dll}")
    if(NOT _one_name IN_LIST _one_windows_dlls)
        message(FATAL_ERROR "Non-system DLL dependency in ${ONE_EXECUTABLE}: ${_one_dll}")
    endif()
endforeach()
message(STATUS "Windows linkage verified: only operating-system DLL imports")
