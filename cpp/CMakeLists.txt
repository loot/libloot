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
# External Projects
##############################

if(MSVC)
    set(LIBLOOT_CPP_FILENAME "libloot_cpp.lib")
else()
    set(LIBLOOT_CPP_FILENAME "liblibloot_cpp.a")
endif()

if(DEFINED RUST_TARGET)
    set(TARGET_PATH "${CMAKE_SOURCE_DIR}/../target/${RUST_TARGET}")
else()
    set(TARGET_PATH "${CMAKE_SOURCE_DIR}/../target")
endif()

add_custom_target(libloot-cpp-build
    COMMAND cargo build
        $<$<NOT:$<CONFIG:Debug>>:--release>
        $<$<AND:$<CONFIG:Debug>,$<PLATFORM_ID:Windows>>:--config>
        $<$<AND:$<CONFIG:Debug>,$<PLATFORM_ID:Windows>>:${CMAKE_SOURCE_DIR}/.cargo/msvc-debug-config.toml>
        $<$<BOOL:${RUST_TARGET}>:--target>
        $<$<BOOL:${RUST_TARGET}>:${RUST_TARGET}>
    BYPRODUCTS
        "${TARGET_PATH}/$<IF:$<CONFIG:Debug>,debug,release>/${LIBLOOT_CPP_FILENAME}"
        "${TARGET_PATH}/cxxbridge/libloot-cpp/src/lib.rs.cc"
        "${TARGET_PATH}/cxxbridge/libloot-cpp/src/lib.rs.h"
        "${TARGET_PATH}/cxxbridge/rust/cxx.h"
)

add_library(libloot-cpp INTERFACE)
add_dependencies(libloot-cpp libloot-cpp-build)

target_link_libraries(libloot-cpp INTERFACE
    optimized "${TARGET_PATH}/release/${LIBLOOT_CPP_FILENAME}"
    debug "${TARGET_PATH}/debug/${LIBLOOT_CPP_FILENAME}")
target_include_directories(libloot-cpp INTERFACE "${TARGET_PATH}/cxxbridge")


##############################
# General Settings
##############################

set(LIBLOOT_SRC_API_CPP_FILES
    "${CMAKE_SOURCE_DIR}/src/api/api.cpp"
    "${CMAKE_SOURCE_DIR}/src/api/convert.cpp"
    "${CMAKE_SOURCE_DIR}/src/api/database.cpp"
    "${CMAKE_SOURCE_DIR}/src/api/exception/cyclic_interaction_error.cpp"
    "${CMAKE_SOURCE_DIR}/src/api/exception/error_categories.cpp"
    "${CMAKE_SOURCE_DIR}/src/api/exception/exception.cpp"
    "${CMAKE_SOURCE_DIR}/src/api/exception/undefined_group_error.cpp"
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
    "${CMAKE_SOURCE_DIR}/src/api/game.cpp"
    "${CMAKE_SOURCE_DIR}/src/api/plugin.cpp"
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
    "${CMAKE_SOURCE_DIR}/src/api/convert.h"
    "${CMAKE_SOURCE_DIR}/src/api/database.h"
    "${CMAKE_SOURCE_DIR}/src/api/exception/exception.h"
    "${CMAKE_SOURCE_DIR}/src/api/game.h"
    "${CMAKE_SOURCE_DIR}/src/api/plugin.h")

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
    "${CMAKE_SOURCE_DIR}/src/api/resource.rc")

##############################
# Define Targets
##############################

# Build API.
add_library(loot ${LIBLOOT_ALL_SOURCES})
target_link_libraries(loot PRIVATE libloot-cpp)

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
    target_compile_definitions(loot PRIVATE UNICODE _UNICODE LOOT_EXPORT)

    set(LOOT_LIBS ntdll ws2_32 bcrypt)

    target_link_libraries(loot PRIVATE ${LOOT_LIBS})
endif()

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(loot PRIVATE "-Wall" "-Wextra")
endif()

if(MSVC)
    # Turn off permissive mode to be more standards-compliant and avoid compiler errors.
    target_compile_options(loot PRIVATE "/permissive-" "/W4" "/Zc:__cplusplus" "/GL")
    target_link_options(loot PRIVATE "/LTCG")
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
    find_package(Git)

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
