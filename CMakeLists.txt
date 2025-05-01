cmake_minimum_required(VERSION 3.24)
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()
if(POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif()
project(libloot)
include(ExternalProject)
include(FetchContent)
include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

option(BUILD_SHARED_LIBS "Build a shared library" ON)
option(RUN_CLANG_TIDY "Whether or not to run clang-tidy during build. Has no effect when using CMake's MSVC generator." OFF)
option(LIBLOOT_BUILD_TESTS "Whether or not to build libloot's tests." ON)
option(LIBLOOT_INSTALL_DOCS "Whether or not to install libloot's docs (which need to be built separately)." ON)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

##############################
# Get Build Revision
##############################

find_package(Git)

if(GIT_FOUND)
    execute_process(COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_COMMIT_STRING
        OUTPUT_STRIP_TRAILING_WHITESPACE)
endif()

if(NOT GIT_COMMIT_STRING)
    set(GIT_COMMIT_STRING "unknown")
endif()

message(STATUS "Git revision: ${GIT_COMMIT_STRING}")

# Write to file.
configure_file("${CMAKE_SOURCE_DIR}/src/api/loot_version.cpp.in" "${CMAKE_BINARY_DIR}/generated/loot_version.cpp" @ONLY)

##############################
# External Projects
##############################

set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)
find_package(Boost REQUIRED CONFIG)

if(NOT DEFINED RUST_TARGET AND WIN32)
    if(MSVC)
        set(RUST_TARGET_ENV "msvc")
    elseif(MINGW)
        set(RUST_TARGET_ENV "gnu")
    endif()

    if(CMAKE_SIZEOF_VOID_P EQUAL 4)
        set(RUST_TARGET_ARCH "i686")
    elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(RUST_TARGET_ARCH "x86_64")
    endif()

    if(DEFINED RUST_TARGET_ENV AND DEFINED RUST_TARGET_ARCH)
        set(RUST_TARGET "${RUST_TARGET_ARCH}-pc-windows-${RUST_TARGET_ENV}")
    endif()
endif()

if(DEFINED RUST_TARGET)
    set(RUST_TARGET_ARGS --target ${RUST_TARGET})
    set(RUST_CLEANUP_COMMAND ${CMAKE_COMMAND} -E rm -r "target/${RUST_TARGET}/release/deps" "target/release")
else()
    set(RUST_CLEANUP_COMMAND ${CMAKE_COMMAND} -E rm -r "target/release/build" "target/release/deps")
endif()

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
    find_package(ICU REQUIRED COMPONENTS data uc)
    find_package(TBB REQUIRED)
endif()

if(NOT DEFINED ESPLUGIN_URL)
    set(ESPLUGIN_URL "https://github.com/Ortham/esplugin/archive/refs/tags/6.1.3.tar.gz")
    set(ESPLUGIN_HASH "SHA256=d11373b9108036e8d8453ff77000ae2961f9d1243c99c42dd732dad0c83d8618")
endif()

ExternalProject_Add(esplugin
    PREFIX "external"
    URL ${ESPLUGIN_URL}
    URL_HASH ${ESPLUGIN_HASH}
    CONFIGURE_COMMAND ""
    BUILD_IN_SOURCE 1
    BUILD_COMMAND cargo build --release --config debug="limited" --manifest-path ffi/Cargo.toml ${RUST_TARGET_ARGS} &&
        cbindgen ffi/ -o ffi/include/esplugin.hpp
    COMMAND ${RUST_CLEANUP_COMMAND}
    INSTALL_COMMAND ""
    BUILD_BYPRODUCTS "<SOURCE_DIR>/target/${RUST_TARGET}/release/${CMAKE_STATIC_LIBRARY_PREFIX}esplugin_ffi${CMAKE_STATIC_LIBRARY_SUFFIX}")
ExternalProject_Get_Property(esplugin SOURCE_DIR)
set(ESPLUGIN_INCLUDE_DIRS "${SOURCE_DIR}/ffi/include")
set(ESPLUGIN_LIBRARIES "${SOURCE_DIR}/target/${RUST_TARGET}/release/${CMAKE_STATIC_LIBRARY_PREFIX}esplugin_ffi${CMAKE_STATIC_LIBRARY_SUFFIX}")
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(ESPLUGIN_LIBRARIES ${ESPLUGIN_LIBRARIES} userenv ntdll)
else()
    set(ESPLUGIN_LIBRARIES ${ESPLUGIN_LIBRARIES} dl)
endif()

if(NOT DEFINED LIBLOADORDER_URL)
    set(LIBLOADORDER_URL "https://github.com/Ortham/libloadorder/archive/refs/tags/18.4.0.tar.gz")
    set(LIBLOADORDER_HASH "SHA256=c83f1f48829e013f2634041b62fed8c4957fb7469a401250b3793953ed3c3d2c")
endif()

ExternalProject_Add(libloadorder
    PREFIX "external"
    URL ${LIBLOADORDER_URL}
    URL_HASH ${LIBLOADORDER_HASH}
    CONFIGURE_COMMAND ""
    BUILD_IN_SOURCE 1
    BUILD_COMMAND cargo build --release --config debug="limited" --manifest-path ffi/Cargo.toml ${RUST_TARGET_ARGS} &&
        cbindgen ffi/ -l c++ -o ffi/include/libloadorder.hpp
    COMMAND ${RUST_CLEANUP_COMMAND}
    INSTALL_COMMAND ""
    BUILD_BYPRODUCTS "<SOURCE_DIR>/target/${RUST_TARGET}/release/${CMAKE_STATIC_LIBRARY_PREFIX}loadorder_ffi${CMAKE_STATIC_LIBRARY_SUFFIX}")
ExternalProject_Get_Property(libloadorder SOURCE_DIR)
set(LIBLOADORDER_INCLUDE_DIRS "${SOURCE_DIR}/ffi/include")
set(LIBLOADORDER_LIBRARIES "${SOURCE_DIR}/target/${RUST_TARGET}/release/${CMAKE_STATIC_LIBRARY_PREFIX}loadorder_ffi${CMAKE_STATIC_LIBRARY_SUFFIX}")
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(LIBLOADORDER_LIBRARIES ${LIBLOADORDER_LIBRARIES} ntdll propsys userenv windowsapp)
else()
    set(LIBLOADORDER_LIBRARIES ${LIBLOADORDER_LIBRARIES} dl)
endif()

if(NOT DEFINED LOOT_CONDITION_INTERPRETER_URL)
    set(LOOT_CONDITION_INTERPRETER_URL "https://github.com/loot/loot-condition-interpreter/archive/refs/tags/5.3.2.tar.gz")
    set(LOOT_CONDITION_INTERPRETER_HASH "SHA256=11a2c62898f6450cdae6db607c63fd7898f62bf67d4874b7ceed3d3693331af3")
endif()

ExternalProject_Add(loot-condition-interpreter
    PREFIX "external"
    URL ${LOOT_CONDITION_INTERPRETER_URL}
    URL_HASH ${LOOT_CONDITION_INTERPRETER_HASH}
    CONFIGURE_COMMAND ""
    BUILD_IN_SOURCE 1
    BUILD_COMMAND cargo build --release --config debug="limited" --manifest-path ffi/Cargo.toml ${RUST_TARGET_ARGS} &&
        cbindgen ffi/ -o ffi/include/loot_condition_interpreter.h
    COMMAND ${RUST_CLEANUP_COMMAND}
    INSTALL_COMMAND ""
    BUILD_BYPRODUCTS "<SOURCE_DIR>/target/${RUST_TARGET}/release/${CMAKE_STATIC_LIBRARY_PREFIX}loot_condition_interpreter_ffi${CMAKE_STATIC_LIBRARY_SUFFIX}")
ExternalProject_Get_Property(loot-condition-interpreter SOURCE_DIR)
set(LCI_INCLUDE_DIRS "${SOURCE_DIR}/ffi/include")
set(LCI_LIBRARIES "${SOURCE_DIR}/target/${RUST_TARGET}/release/${CMAKE_STATIC_LIBRARY_PREFIX}loot_condition_interpreter_ffi${CMAKE_STATIC_LIBRARY_SUFFIX}")
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(LCI_LIBRARIES ${LCI_LIBRARIES} userenv)
else()
    set(LCI_LIBRARIES ${LCI_LIBRARIES} dl)
endif()

FetchContent_Declare(
    fmt
    URL "https://github.com/fmtlib/fmt/archive/refs/tags/11.1.4.tar.gz"
    URL_HASH "SHA256=ac366b7b4c2e9f0dde63a59b3feb5ee59b67974b14ee5dc9ea8ad78aa2c1ee1e"
    FIND_PACKAGE_ARGS)

set(SPDLOG_FMT_EXTERNAL ON)
FetchContent_Declare(
    spdlog
    URL "https://github.com/gabime/spdlog/archive/v1.15.2.tar.gz"
    URL_HASH "SHA256=7a80896357f3e8e920e85e92633b14ba0f229c506e6f978578bdc35ba09e9a5d"
    FIND_PACKAGE_ARGS)

set(YAML_MSVC_SHARED_RT ON)
set(YAML_CPP_BUILD_CONTRIB OFF)
set(YAML_CPP_BUILD_TOOLS OFF)
set(YAML_BUILD_SHARED_LIBS OFF)
set(YAML_CPP_BUILD_TESTS OFF)
# Suppress additional targets added by CTest, which is pulled in by yaml-cpp.
set_property(GLOBAL PROPERTY CTEST_TARGETS_ADDED 1)
FetchContent_Declare(
    yaml-cpp
    URL "https://github.com/loot/yaml-cpp/archive/0.8.0+merge-key-support.3.tar.gz"
    URL_HASH "SHA256=e2067f1ab8f658aeb8a8795fd0e06a753b4150b9352cae047280e659078dd44f")

# Set BUILD_SHARED_LIBS=OFF to prevent fmt and spdlog from being built as shared
# libraries.
set(LIBLOOT_BUILD_SHARED_LIBS ${BUILD_SHARED_LIBS})
set(BUILD_SHARED_LIBS OFF)

FetchContent_MakeAvailable(fmt spdlog yaml-cpp)

# Restore the original value of BUILD_SHARED_LIBS.
set(BUILD_SHARED_LIBS ${LIBLOOT_BUILD_SHARED_LIBS})

##############################
# General Settings
##############################

set(LIBLOOT_SRC_API_CPP_FILES
    "${CMAKE_SOURCE_DIR}/src/api/api.cpp"
    "${CMAKE_SOURCE_DIR}/src/api/api_database.cpp"
    "${CMAKE_SOURCE_DIR}/src/api/bsa.cpp"
    "${CMAKE_SOURCE_DIR}/src/api/exception/cyclic_interaction_error.cpp"
    "${CMAKE_SOURCE_DIR}/src/api/exception/error_categories.cpp"
    "${CMAKE_SOURCE_DIR}/src/api/exception/undefined_group_error.cpp"
    "${CMAKE_SOURCE_DIR}/src/api/metadata/condition_evaluator.cpp"
    "${CMAKE_SOURCE_DIR}/src/api/metadata/conditional_metadata.cpp"
    "${CMAKE_SOURCE_DIR}/src/api/metadata/file.cpp"
    "${CMAKE_SOURCE_DIR}/src/api/metadata/filename.cpp"
    "${CMAKE_SOURCE_DIR}/src/api/metadata/group.cpp"
    "${CMAKE_SOURCE_DIR}/src/api/metadata/location.cpp"
    "${CMAKE_SOURCE_DIR}/src/api/metadata/message.cpp"
    "${CMAKE_SOURCE_DIR}/src/api/metadata/message_content.cpp"
    "${CMAKE_SOURCE_DIR}/src/api/metadata/plugin_cleaning_data.cpp"
    "${CMAKE_SOURCE_DIR}/src/api/metadata/plugin_metadata.cpp"
    "${CMAKE_SOURCE_DIR}/src/api/metadata/tag.cpp"
    "${CMAKE_SOURCE_DIR}/src/api/game/game.cpp"
    "${CMAKE_SOURCE_DIR}/src/api/game/game_cache.cpp"
    "${CMAKE_SOURCE_DIR}/src/api/game/load_order_handler.cpp"
    "${CMAKE_SOURCE_DIR}/src/api/metadata_list.cpp"
    "${CMAKE_SOURCE_DIR}/src/api/plugin.cpp"
    "${CMAKE_SOURCE_DIR}/src/api/sorting/group_sort.cpp"
    "${CMAKE_SOURCE_DIR}/src/api/sorting/plugin_sort.cpp"
    "${CMAKE_SOURCE_DIR}/src/api/sorting/plugin_graph.cpp"
    "${CMAKE_SOURCE_DIR}/src/api/sorting/plugin_sorting_data.cpp"
    "${CMAKE_SOURCE_DIR}/src/api/helpers/crc.cpp"
    "${CMAKE_SOURCE_DIR}/src/api/helpers/logging.cpp"
    "${CMAKE_SOURCE_DIR}/src/api/helpers/text.cpp"
    "${CMAKE_SOURCE_DIR}/src/api/vertex.cpp")

set(LIBLOOT_INCLUDE_H_FILES
    "${CMAKE_SOURCE_DIR}/include/loot/api.h"
    "${CMAKE_SOURCE_DIR}/include/loot/api_decorator.h"
    "${CMAKE_SOURCE_DIR}/include/loot/database_interface.h"
    "${CMAKE_SOURCE_DIR}/include/loot/exception/error_categories.h"
    "${CMAKE_SOURCE_DIR}/include/loot/exception/condition_syntax_error.h"
    "${CMAKE_SOURCE_DIR}/include/loot/exception/cyclic_interaction_error.h"
    "${CMAKE_SOURCE_DIR}/include/loot/exception/file_access_error.h"
    "${CMAKE_SOURCE_DIR}/include/loot/exception/undefined_group_error.h"
    "${CMAKE_SOURCE_DIR}/include/loot/enum/edge_type.h"
    "${CMAKE_SOURCE_DIR}/include/loot/enum/game_type.h"
    "${CMAKE_SOURCE_DIR}/include/loot/enum/log_level.h"
    "${CMAKE_SOURCE_DIR}/include/loot/enum/message_type.h"
    "${CMAKE_SOURCE_DIR}/include/loot/game_interface.h"
    "${CMAKE_SOURCE_DIR}/include/loot/loot_version.h"
    "${CMAKE_SOURCE_DIR}/include/loot/metadata/conditional_metadata.h"
    "${CMAKE_SOURCE_DIR}/include/loot/metadata/file.h"
    "${CMAKE_SOURCE_DIR}/include/loot/metadata/filename.h"
    "${CMAKE_SOURCE_DIR}/include/loot/metadata/group.h"
    "${CMAKE_SOURCE_DIR}/include/loot/metadata/location.h"
    "${CMAKE_SOURCE_DIR}/include/loot/metadata/message.h"
    "${CMAKE_SOURCE_DIR}/include/loot/metadata/message_content.h"
    "${CMAKE_SOURCE_DIR}/include/loot/metadata/plugin_cleaning_data.h"
    "${CMAKE_SOURCE_DIR}/include/loot/metadata/plugin_metadata.h"
    "${CMAKE_SOURCE_DIR}/include/loot/metadata/tag.h"
    "${CMAKE_SOURCE_DIR}/include/loot/plugin_interface.h"
    "${CMAKE_SOURCE_DIR}/include/loot/vertex.h")

set(LIBLOOT_SRC_API_H_FILES
    "${CMAKE_SOURCE_DIR}/src/api/api_database.h"
    "${CMAKE_SOURCE_DIR}/src/api/bsa.h"
    "${CMAKE_SOURCE_DIR}/src/api/bsa_detail.h"
    "${CMAKE_SOURCE_DIR}/src/api/metadata/condition_evaluator.h"
    "${CMAKE_SOURCE_DIR}/src/api/metadata/yaml/file.h"
    "${CMAKE_SOURCE_DIR}/src/api/metadata/yaml/group.h"
    "${CMAKE_SOURCE_DIR}/src/api/metadata/yaml/location.h"
    "${CMAKE_SOURCE_DIR}/src/api/metadata/yaml/message.h"
    "${CMAKE_SOURCE_DIR}/src/api/metadata/yaml/message_content.h"
    "${CMAKE_SOURCE_DIR}/src/api/metadata/yaml/plugin_cleaning_data.h"
    "${CMAKE_SOURCE_DIR}/src/api/metadata/yaml/plugin_metadata.h"
    "${CMAKE_SOURCE_DIR}/src/api/metadata/yaml/set.h"
    "${CMAKE_SOURCE_DIR}/src/api/metadata/yaml/tag.h"
    "${CMAKE_SOURCE_DIR}/src/api/game/game.h"
    "${CMAKE_SOURCE_DIR}/src/api/game/game_cache.h"
    "${CMAKE_SOURCE_DIR}/src/api/game/load_order_handler.h"
    "${CMAKE_SOURCE_DIR}/src/api/metadata_list.h"
    "${CMAKE_SOURCE_DIR}/src/api/plugin.h"
    "${CMAKE_SOURCE_DIR}/src/api/sorting/group_sort.h"
    "${CMAKE_SOURCE_DIR}/src/api/sorting/plugin_sort.h"
    "${CMAKE_SOURCE_DIR}/src/api/sorting/plugin_graph.h"
    "${CMAKE_SOURCE_DIR}/src/api/sorting/plugin_sorting_data.h"
    "${CMAKE_SOURCE_DIR}/src/api/helpers/crc.h"
    "${CMAKE_SOURCE_DIR}/src/api/helpers/logging.h"
    "${CMAKE_SOURCE_DIR}/src/api/helpers/text.h")

source_group(TREE "${CMAKE_SOURCE_DIR}/src/api"
    PREFIX "Source Files"
    FILES ${LIBLOOT_SRC_API_CPP_FILES})

source_group(TREE "${CMAKE_SOURCE_DIR}/include"
    PREFIX "Header Files"
    FILES ${LIBLOOT_INCLUDE_H_FILES})

source_group(TREE "${CMAKE_SOURCE_DIR}/src/api"
    PREFIX "Header Files"
    FILES ${LIBLOOT_SRC_API_H_FILES})

set(LIBLOOT_ALL_SOURCES
    ${LIBLOOT_SRC_API_CPP_FILES}
    ${LIBLOOT_INCLUDE_H_FILES}
    ${LIBLOOT_SRC_API_H_FILES}
    "${CMAKE_BINARY_DIR}/generated/loot_version.cpp"
    "${CMAKE_SOURCE_DIR}/src/api/resource.rc")

##############################
# Define Targets
##############################

# Build API.
add_library(loot ${LIBLOOT_ALL_SOURCES})
add_dependencies(loot
    esplugin
    libloadorder
    loot-condition-interpreter)
target_link_libraries(loot PRIVATE
    Boost::headers
    ${ESPLUGIN_LIBRARIES}
    ${LIBLOADORDER_LIBRARIES}
    ${LCI_LIBRARIES}
    fmt::fmt
    spdlog::spdlog
    yaml-cpp::yaml-cpp)

##############################
# Set Target-Specific Flags
##############################

set(LIBLOOT_INCLUDE_DIRS
    "${CMAKE_SOURCE_DIR}/src"
    "${CMAKE_SOURCE_DIR}/include")

set(LIBLOOT_COMMON_SYSTEM_INCLUDE_DIRS
    ${LIBLOADORDER_INCLUDE_DIRS}
    ${ESPLUGIN_INCLUDE_DIRS}
    ${LCI_INCLUDE_DIRS})

target_include_directories(loot PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")
target_include_directories(loot PRIVATE ${LIBLOOT_INCLUDE_DIRS})
target_include_directories(loot SYSTEM PRIVATE
    ${LIBLOOT_COMMON_SYSTEM_INCLUDE_DIRS})

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    target_compile_definitions(loot PRIVATE
        UNICODE _UNICODE LOOT_EXPORT YAML_CPP_STATIC_DEFINE)

    set(LOOT_LIBS ws2_32 bcrypt)

    if(NOT CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
        set(LOOT_LIBS ${LOOT_LIBS} tbb_static)
    endif()

    target_link_libraries(loot PRIVATE ${LOOT_LIBS})
else()
    set(LOOT_LIBS ICU::data ICU::uc pthread TBB::tbb)

    target_link_libraries(loot PUBLIC ${LOOT_LIBS})
endif()

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(loot PRIVATE "-Wall" "-Wextra")
endif()

if(MSVC)
    # Turn off permissive mode to be more standards-compliant and avoid compiler errors.
    target_compile_options(loot PRIVATE "/Zc:__cplusplus" "/permissive-" "/W4")
endif()

##############################
# Configure clang-tidy
##############################

if(RUN_CLANG_TIDY)
    set(CLANG_TIDY_COMMON_CHECKS
        "cppcoreguidelines-avoid-c-arrays"
        "cppcoreguidelines-c-copy-assignment-signature"
        "cppcoreguidelines-explicit-virtual-functions"
        "cppcoreguidelines-init-variables"
        "cppcoreguidelines-interfaces-global-init"
        "cppcoreguidelines-macro-usage"
        "cppcoreguidelines-narrowing-conventions"
        "cppcoreguidelines-no-malloc"
        "cppcoreguidelines-pro-bounds-array-to-pointer-decay"
        "cppcoreguidelines-pro-bounds-constant-array-index"
        "cppcoreguidelines-pro-bounds-pointer-arithmetic"
        "cppcoreguidelines-pro-type-const-cast"
        "cppcoreguidelines-pro-type-cstyle-cast"
        "cppcoreguidelines-pro-type-member-init"
        "cppcoreguidelines-pro-type-reinterpret-cast"
        "cppcoreguidelines-pro-type-static-cast-downcast"
        "cppcoreguidelines-pro-type-union-access"
        "cppcoreguidelines-pro-type-vararg"
        "cppcoreguidelines-pro-type-slicing")

    set(CLANG_TIDY_LIB_CHECKS
        ${CLANG_TIDY_COMMON_CHECKS}
        "cppcoreguidelines-avoid-goto"
        "cppcoreguidelines-avoid-magic-numbers"
        "cppcoreguidelines-non-private-member-variables-in-classes"
        "cppcoreguidelines-special-member-functions")

    list(JOIN CLANG_TIDY_LIB_CHECKS "," CLANG_TIDY_LIB_CHECKS_JOINED)

    set(CLANG_TIDY_LIB
        clang-tidy "-header-filter=.*" "-checks=${CLANG_TIDY_LIB_CHECKS_JOINED}")

    set_target_properties(loot
        PROPERTIES
            CXX_CLANG_TIDY "${CLANG_TIDY_LIB}")
endif()

##############################
# Tests
##############################

if(LIBLOOT_BUILD_TESTS)
    include("cmake/tests.cmake")
endif()

########################################
# Install
########################################

set(LIBLOOT_VERSION "0.26.1")

set_property(TARGET loot PROPERTY VERSION ${LIBLOOT_VERSION})
set_property(TARGET loot PROPERTY SOVERSION 0)
set_property(TARGET loot PROPERTY INTERFACE_libloot_MAJOR_VERSION 0)
set_property(TARGET loot APPEND PROPERTY COMPATIBLE_INTERFACE_STRING libloot_MAJOR_VERSION)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/Config.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/liblootConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/libloot
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/liblootConfigVersion.cmake"
    VERSION "${LIBLOOT_VERSION}"
    COMPATIBILITY AnyNewerVersion
)

install(TARGETS loot
    EXPORT liblootTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

if(MSVC)
    install(FILES $<TARGET_PDB_FILE:loot>
        DESTINATION ${CMAKE_INSTALL_LIBDIR}
        OPTIONAL
        CONFIGURATIONS RelWithDebInfo)
endif()

install(DIRECTORY "${CMAKE_SOURCE_DIR}/include/"
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

if(LIBLOOT_INSTALL_DOCS)
    install(DIRECTORY "${CMAKE_BINARY_DIR}/docs/html/"
        DESTINATION ${CMAKE_INSTALL_DOCDIR})
endif()

install(EXPORT liblootTargets
    FILE liblootTargets.cmake
    NAMESPACE libloot::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/libloot)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/liblootConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/liblootConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/libloot)

########################################
# CPack
########################################

if(NOT DEFINED CPACK_PACKAGE_VERSION)
    if(GIT_FOUND)
        execute_process(COMMAND ${GIT_EXECUTABLE} describe --tags --long --always --abbrev=7
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            OUTPUT_VARIABLE GIT_DESCRIBE_STRING
            OUTPUT_STRIP_TRAILING_WHITESPACE)
    else()
        set(GIT_DESCRIBE_STRING "unknown-version")
    endif()

    set(CPACK_PACKAGE_VERSION ${GIT_DESCRIBE_STRING})
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(CPACK_GENERATOR "7Z")
else()
    set(CPACK_GENERATOR "TXZ")
endif()

set(CPACK_PACKAGE_DIRECTORY "${CMAKE_BINARY_DIR}/package")

include(CPack)
