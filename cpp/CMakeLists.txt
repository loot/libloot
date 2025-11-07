cmake_minimum_required(VERSION 3.24)
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()
if(POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif()
project(libloot VERSION "0.28.3")
include(CMakeDependentOption)
include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

cmake_dependent_option(LIBLOOT_BUILD_SHARED
    "Build a shared library"
    ON
    "NOT BUILD_SHARED_LIBS"
    ON)
option(RUN_CLANG_TIDY "Whether or not to run clang-tidy during build. Has no effect when using CMake's MSVC generator." OFF)
option(LIBLOOT_BUILD_TESTS "Whether or not to build libloot's tests." ON)
option(LIBLOOT_INSTALL_DOCS "Whether or not to install libloot's docs (which need to be built separately)." ON)

##############################
# External Projects
##############################

if(MSVC)
    set(LIBLOOT_CPP_FILENAME "libloot_cpp.lib")
else()
    set(LIBLOOT_CPP_FILENAME "liblibloot_cpp.a")
endif()

if(DEFINED RUST_TARGET)
    set(TARGET_PATH "${PROJECT_SOURCE_DIR}/../target/${RUST_TARGET}")
else()
    set(TARGET_PATH "${PROJECT_SOURCE_DIR}/../target")
endif()

set(CARGO_COMMAND cargo build
    $<$<NOT:$<CONFIG:Debug>>:--release>
    $<$<AND:$<CONFIG:Debug>,$<PLATFORM_ID:Windows>>:--config>
    $<$<AND:$<CONFIG:Debug>,$<PLATFORM_ID:Windows>>:${PROJECT_SOURCE_DIR}/.cargo/msvc-debug-config.toml>
    $<$<BOOL:${RUST_TARGET}>:--target>
    $<$<BOOL:${RUST_TARGET}>:${RUST_TARGET}>)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(CARGO_COMMAND ${CMAKE_COMMAND} -E env --modify "CXXFLAGS=string_append: -fno-lto" -- ${CARGO_COMMAND})
endif()

add_custom_target(libloot-cpp-build
    # Force LTO to be disabled when building the first layer of the C++ wrapper,
    # as it may be enabled by default, and if so that leads to undefined symbols
    # in the C++ wrapper's shared library. It's not enough to just pass -fno-lto
    # when building libloot-cpp itself, it also needs to be passed when building
    # the cxx crate, as otherwise there will be missing cxxbridge symbols.
    COMMAND ${CARGO_COMMAND}
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
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
    "${PROJECT_SOURCE_DIR}/src/api/api.cpp"
    "${PROJECT_SOURCE_DIR}/src/api/convert.cpp"
    "${PROJECT_SOURCE_DIR}/src/api/database.cpp"
    "${PROJECT_SOURCE_DIR}/src/api/exception/cyclic_interaction_error.cpp"
    "${PROJECT_SOURCE_DIR}/src/api/exception/exception.cpp"
    "${PROJECT_SOURCE_DIR}/src/api/exception/undefined_group_error.cpp"
    "${PROJECT_SOURCE_DIR}/src/api/metadata/file.cpp"
    "${PROJECT_SOURCE_DIR}/src/api/metadata/filename.cpp"
    "${PROJECT_SOURCE_DIR}/src/api/metadata/group.cpp"
    "${PROJECT_SOURCE_DIR}/src/api/metadata/location.cpp"
    "${PROJECT_SOURCE_DIR}/src/api/metadata/message.cpp"
    "${PROJECT_SOURCE_DIR}/src/api/metadata/message_content.cpp"
    "${PROJECT_SOURCE_DIR}/src/api/metadata/plugin_cleaning_data.cpp"
    "${PROJECT_SOURCE_DIR}/src/api/metadata/plugin_metadata.cpp"
    "${PROJECT_SOURCE_DIR}/src/api/metadata/tag.cpp"
    "${PROJECT_SOURCE_DIR}/src/api/game.cpp"
    "${PROJECT_SOURCE_DIR}/src/api/plugin.cpp"
    "${PROJECT_SOURCE_DIR}/src/api/vertex.cpp")

set(LIBLOOT_INCLUDE_H_FILES
    "${PROJECT_SOURCE_DIR}/include/loot/api.h"
    "${PROJECT_SOURCE_DIR}/include/loot/api_decorator.h"
    "${PROJECT_SOURCE_DIR}/include/loot/database_interface.h"
    "${PROJECT_SOURCE_DIR}/include/loot/exception/cyclic_interaction_error.h"
    "${PROJECT_SOURCE_DIR}/include/loot/exception/plugin_not_loaded_error.h"
    "${PROJECT_SOURCE_DIR}/include/loot/exception/undefined_group_error.h"
    "${PROJECT_SOURCE_DIR}/include/loot/enum/edge_type.h"
    "${PROJECT_SOURCE_DIR}/include/loot/enum/game_type.h"
    "${PROJECT_SOURCE_DIR}/include/loot/enum/log_level.h"
    "${PROJECT_SOURCE_DIR}/include/loot/enum/message_type.h"
    "${PROJECT_SOURCE_DIR}/include/loot/game_interface.h"
    "${PROJECT_SOURCE_DIR}/include/loot/loot_version.h"
    "${PROJECT_SOURCE_DIR}/include/loot/metadata/file.h"
    "${PROJECT_SOURCE_DIR}/include/loot/metadata/filename.h"
    "${PROJECT_SOURCE_DIR}/include/loot/metadata/group.h"
    "${PROJECT_SOURCE_DIR}/include/loot/metadata/location.h"
    "${PROJECT_SOURCE_DIR}/include/loot/metadata/message.h"
    "${PROJECT_SOURCE_DIR}/include/loot/metadata/message_content.h"
    "${PROJECT_SOURCE_DIR}/include/loot/metadata/plugin_cleaning_data.h"
    "${PROJECT_SOURCE_DIR}/include/loot/metadata/plugin_metadata.h"
    "${PROJECT_SOURCE_DIR}/include/loot/metadata/tag.h"
    "${PROJECT_SOURCE_DIR}/include/loot/plugin_interface.h"
    "${PROJECT_SOURCE_DIR}/include/loot/vertex.h")

set(LIBLOOT_SRC_API_H_FILES
    "${PROJECT_SOURCE_DIR}/src/api/convert.h"
    "${PROJECT_SOURCE_DIR}/src/api/database.h"
    "${PROJECT_SOURCE_DIR}/src/api/exception/exception.h"
    "${PROJECT_SOURCE_DIR}/src/api/game.h"
    "${PROJECT_SOURCE_DIR}/src/api/plugin.h")

source_group(TREE "${PROJECT_SOURCE_DIR}/src/api"
    PREFIX "Source Files"
    FILES ${LIBLOOT_SRC_API_CPP_FILES})

source_group(TREE "${PROJECT_SOURCE_DIR}/include"
    PREFIX "Header Files"
    FILES ${LIBLOOT_INCLUDE_H_FILES})

source_group(TREE "${PROJECT_SOURCE_DIR}/src/api"
    PREFIX "Header Files"
    FILES ${LIBLOOT_SRC_API_H_FILES})

set(LIBLOOT_ALL_SOURCES
    ${LIBLOOT_SRC_API_CPP_FILES}
    ${LIBLOOT_INCLUDE_H_FILES}
    ${LIBLOOT_SRC_API_H_FILES}
    "${PROJECT_SOURCE_DIR}/src/api/resource.rc")

##############################
# Define Targets
##############################

if(LIBLOOT_BUILD_SHARED)
    set(LIBLOOT_LIBRARY_TYPE "SHARED")
else()
    set(LIBLOOT_LIBRARY_TYPE "STATIC")
endif()

# Build API.
add_library(loot ${LIBLOOT_LIBRARY_TYPE} ${LIBLOOT_ALL_SOURCES})
target_link_libraries(loot PRIVATE libloot-cpp)

##############################
# Set Target-Specific Flags
##############################

set(LIBLOOT_INCLUDE_DIRS
    "${PROJECT_SOURCE_DIR}/src"
    "${PROJECT_SOURCE_DIR}/include")

set(LIBLOOT_COMMON_SYSTEM_INCLUDE_DIRS
    ${LIBLOADORDER_INCLUDE_DIRS}
    ${ESPLUGIN_INCLUDE_DIRS}
    ${LCI_INCLUDE_DIRS})

target_include_directories(loot PUBLIC
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")
target_include_directories(loot PRIVATE ${LIBLOOT_INCLUDE_DIRS})
target_include_directories(loot SYSTEM PRIVATE
    ${LIBLOOT_COMMON_SYSTEM_INCLUDE_DIRS})

set_target_properties(loot PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON)

if(WIN32)
    target_compile_definitions(loot PRIVATE UNICODE _UNICODE LOOT_EXPORT)

    set(LOOT_LIBS ntdll ws2_32 bcrypt)

    target_link_libraries(loot PRIVATE ${LOOT_LIBS})
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(loot PRIVATE
        "-Wall"
        "-Wextra"
        "-Wpedantic"
        "-Wconversion"
        "-Wdeprecated"
        "-Wuninitialized"
        "-Winit-self"
        "-Wswitch-enum"
        "-Wstrict-overflow=5"
        "-Wstringop-overflow=4"
        "-Wduplicated-branches"
        "-Wduplicated-cond"
        "-Wzero-as-null-pointer-constant"
        "-Wfloat-equal"
        "-Wshadow"
        "-Wcast-qual"
        "-Wcast-align"
        "-Wsign-conversion"
        "-Wlogical-op"
        "-Wvla"
        "-Wno-xor-used-as-pow")
endif()

if(MSVC)
    # Turn off permissive mode to be more standards-compliant and avoid compiler errors.
    target_compile_options(loot PRIVATE
        "/permissive-"
        "/W4"
        "/Wall"
        "/wd4514"
        "/wd4623"
        "/wd4625"
        "/wd4626"
        "/wd4710"
        "/wd4711"
        "/wd4738"
        "/wd4820"
        "/wd5026"
        "/wd5027"
        "/wd5045"
        "/external:anglebrackets"
        "/external:W0"
        "/Zc:__cplusplus"
        "/GL"
        "/MP"
        "/sdl"
        "$<$<CONFIG:Debug>:/RTC1>")
    target_link_options(loot PRIVATE "/INCREMENTAL:NO" "/LTCG")
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

add_library(libloot::loot ALIAS loot)

if(LIBLOOT_BUILD_SHARED)
    set_property(TARGET loot PROPERTY VERSION ${libloot_VERSION})
    set_property(TARGET loot PROPERTY SOVERSION ${libloot_VERSION_MAJOR})
    set_property(TARGET loot PROPERTY INTERFACE_libloot_MAJOR_VERSION ${libloot_VERSION_MAJOR})
    set_property(TARGET loot APPEND PROPERTY COMPATIBLE_INTERFACE_STRING libloot_MAJOR_VERSION)

    configure_package_config_file(
        ${PROJECT_SOURCE_DIR}/cmake/Config.cmake.in
        "${PROJECT_BINARY_DIR}/liblootConfig.cmake"
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/libloot
    )

    write_basic_package_version_file(
        "${PROJECT_BINARY_DIR}/liblootConfigVersion.cmake"
        VERSION "${libloot_VERSION}"
        COMPATIBILITY AnyNewerVersion
    )

    install(TARGETS loot
        EXPORT liblootTargets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

    install(EXPORT liblootTargets
        NAMESPACE libloot::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/libloot)

    export(TARGETS loot
        NAMESPACE libloot::
        FILE "${PROJECT_BINARY_DIR}/liblootTargets.cmake")

    if(MSVC)
        install(FILES $<TARGET_PDB_FILE:loot>
            DESTINATION ${CMAKE_INSTALL_LIBDIR}
            OPTIONAL
            CONFIGURATIONS RelWithDebInfo)
    endif()

    install(DIRECTORY "${PROJECT_SOURCE_DIR}/include/"
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

    if(LIBLOOT_INSTALL_DOCS)
        install(DIRECTORY "${PROJECT_SOURCE_DIR}/../docs/build/html/"
            DESTINATION ${CMAKE_INSTALL_DOCDIR})
    endif()

    install(FILES
        "${PROJECT_BINARY_DIR}/liblootConfig.cmake"
        "${PROJECT_BINARY_DIR}/liblootConfigVersion.cmake"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/libloot)
endif()

########################################
# CPack
########################################

if(NOT DEFINED CPACK_PACKAGE_VERSION)
    find_package(Git)

    if(GIT_FOUND)
        execute_process(COMMAND ${GIT_EXECUTABLE} describe --tags --long --always --abbrev=7
            WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
            OUTPUT_VARIABLE GIT_DESCRIBE_STRING
            OUTPUT_STRIP_TRAILING_WHITESPACE)
    else()
        set(GIT_DESCRIBE_STRING "unknown-version")
    endif()

    set(CPACK_PACKAGE_VERSION ${GIT_DESCRIBE_STRING})
endif()

if(WIN32)
    set(CPACK_GENERATOR "7Z")
else()
    set(CPACK_GENERATOR "TXZ")
endif()

set(CPACK_PACKAGE_DIRECTORY "${PROJECT_BINARY_DIR}/package")

include(CPack)
