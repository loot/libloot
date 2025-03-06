cmake_minimum_required(VERSION 2.8)
project(ffi_tests CXX)

include_directories("${CMAKE_SOURCE_DIR}/../target/cxxbridge")

set(CMAKE_CXX_STANDARD 17)

include(FetchContent)
include(GoogleTest)

set(BUILD_GMOCK OFF)
set(gtest_force_shared_crt ON)
set(INSTALL_GTEST OFF)
# Google Test doesn't have a scoped equivalent to BUILD_SHARED_LIBS, so set the
# global value and then set it back to the original value once Google Test has
# been configured.
set(BUILD_SHARED_LIBS_INITIAL ${BUILD_SHARED_LIBS})
set(BUILD_SHARED_LIBS OFF)
FetchContent_Declare(
    GTest
    URL "https://github.com/google/googletest/archive/refs/tags/v1.15.2.tar.gz"
    URL_HASH "SHA256=7b42b4d6ed48810c5362c265a17faebe90dc2373c885e5216439d37927f02926"
    FIND_PACKAGE_ARGS)

FetchContent_MakeAvailable(GTest)

if (CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set (SYSTEM_LIBS pthread dl)
endif ()

if (MSVC)
    set (SYSTEM_LIBS ntdll ws2_32 Userenv bcrypt)
endif ()

set (FFI_LIBRARY "${CMAKE_SOURCE_DIR}/../target/debug/libloot_cxx.lib")

add_executable(ffi_cpp_tests "${CMAKE_SOURCE_DIR}/tests/ffi.cpp")
target_link_libraries(ffi_cpp_tests ${FFI_LIBRARY} ${SYSTEM_LIBS} GTest::gtest_main)

enable_testing()
gtest_discover_tests(ffi_cpp_tests DISCOVERY_TIMEOUT 10)
