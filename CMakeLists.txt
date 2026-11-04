cmake_minimum_required(VERSION 3.24)

project(WinActivateManager VERSION 0.2.0 LANGUAGES CXX RC)

if(NOT WIN32)
    message(FATAL_ERROR "WinActivate Manager supports Windows only.")
endif()

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "WinActivate Manager supports x64 builds only.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(winactivate_common STATIC
    src/common/FormatError.cpp
    src/common/KeyValidation.cpp
    src/common/KmsOptions.cpp
    src/common/IpcProtocol.cpp
)
target_include_directories(winactivate_common PUBLIC src)
target_compile_definitions(winactivate_common PUBLIC
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    _WIN32_WINNT=0x0601
)

add_library(winactivate_core STATIC
    src/core/ActivationWorkflow.cpp
    src/core/KmsWorkflow.cpp
    src/core/LicenseActivator.cpp
    src/core/LicenseReader.cpp
    src/core/MockLicensingBackend.cpp
    src/core/OsDetector.cpp
    src/core/ProductSelection.cpp
    src/core/WmiConnection.cpp
)
target_include_directories(winactivate_core PUBLIC src)
target_link_libraries(winactivate_core PUBLIC
    winactivate_common
    wbemuuid
    ole32
    oleaut32
)
target_compile_definitions(winactivate_core PUBLIC
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

add_executable(WinActivate WIN32
    src/gui/Main.cpp
    src/gui/MainWindow.cpp
    src/gui/Resources.rc
    src/common/HelperTransport.cpp
)
target_link_libraries(WinActivate PRIVATE
    winactivate_core
    winactivate_common
    comctl32
    shell32
    advapi32
)
set_target_properties(WinActivate PROPERTIES OUTPUT_NAME "WinActivate")
target_include_directories(WinActivate PRIVATE src/gui)

add_executable(WinActivateHelper WIN32
    src/helper/HelperMain.cpp
    src/helper/Resources.rc
    src/common/HelperTransport.cpp
)
target_link_libraries(WinActivateHelper PRIVATE winactivate_core shell32 advapi32)
target_include_directories(WinActivateHelper PRIVATE src/helper)
if(MSVC)
    # Manifests are embedded by the resource compiler, not generated a second time.
    target_link_options(WinActivate PRIVATE /MANIFEST:NO)
    target_link_options(WinActivateHelper PRIVATE /MANIFEST:NO)
else()
    target_link_options(WinActivate PRIVATE -municode)
    target_link_options(WinActivateHelper PRIVATE -municode)
    target_link_libraries(WinActivate PRIVATE gdi32)
endif()

if(MSVC)
    foreach(target winactivate_common winactivate_core WinActivate WinActivateHelper)
        target_compile_options(${target} PRIVATE /W4 /permissive- /EHsc /utf-8)
    endforeach()
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(WinActivateTests
        tests/TestsMain.cpp
        tests/KeyValidationTests.cpp
        tests/MockBackendTests.cpp
        tests/OsDetectionTests.cpp
        tests/ProductSelectionTests.cpp
        tests/ErrorMappingTests.cpp
        tests/KmsWorkflowTests.cpp
        tests/IpcProtocolTests.cpp
        tests/HelperTransportTests.cpp
        src/common/HelperTransport.cpp
    )
    target_link_libraries(WinActivateTests PRIVATE winactivate_core winactivate_common shell32 advapi32)
    if(MSVC)
        target_compile_options(WinActivateTests PRIVATE /W4 /permissive- /EHsc /utf-8)
    endif()
    add_test(NAME WinActivate.UnitTests COMMAND WinActivateTests)
endif()

