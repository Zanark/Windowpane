cmake_minimum_required(VERSION 3.24)
project(WindowPane VERSION 0.1.0 LANGUAGES CXX RC)

if(NOT WIN32 OR NOT MSVC)
    message(FATAL_ERROR "WindowPane requires Windows and the MSVC Windows SDK toolchain.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
add_compile_definitions(UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
add_compile_options(/W4 /WX /permissive- /EHsc /utf-8 /Zc:__cplusplus)

add_library(WindowPaneCore src/pixels.cpp)
target_include_directories(WindowPaneCore PUBLIC src)

add_executable(WindowPane WIN32 src/app.cpp src/capture.cpp src/app.rc)
target_link_options(WindowPane PRIVATE /MANIFEST:NO)
target_link_libraries(WindowPane PRIVATE WindowPaneCore
    windowsapp d3d11 dxgi dwmapi comctl32 shell32 user32 gdi32 ole32 psapi uxtheme)

include(CTest)
if(BUILD_TESTING)
    add_executable(WindowPaneTests tests/pixels_tests.cpp)
    target_link_libraries(WindowPaneTests PRIVATE WindowPaneCore)
    add_test(NAME pixels COMMAND WindowPaneTests)
    add_test(NAME native-capture-and-controls
        COMMAND WindowPane --self-test "${CMAKE_BINARY_DIR}/native-test.txt")
    set_tests_properties(native-capture-and-controls PROPERTIES TIMEOUT 60)
endif()
