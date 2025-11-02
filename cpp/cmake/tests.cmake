##############################
# Dependencies
##############################

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
    URL "https://github.com/google/googletest/archive/refs/tags/v1.16.0.tar.gz"
    URL_HASH "SHA256=78c676fc63881529bf97bf9d45948d905a66833fbfa5318ea2cd7478cb98f399"
    FIND_PACKAGE_ARGS)

FetchContent_Declare(
    testing-plugins
    URL "https://github.com/Ortham/testing-plugins/archive/1.6.2.tar.gz"
    URL_HASH "SHA256=f6e5b55e2669993ab650ba470424b725d1fab71ace979134a77de3373bd55620")

FetchContent_MakeAvailable(GTest testing-plugins)

set(BUILD_SHARED_LIBS ${BUILD_SHARED_LIBS_INITIAL})


##############################
# General Settings
##############################

set(LIBLOOT_SRC_TESTS_INTERFACE_CPP_FILES
    "${PROJECT_SOURCE_DIR}/src/tests/api/interface/main.cpp")

set(LIBLOOT_SRC_TESTS_INTERFACE_H_FILES
    "${PROJECT_SOURCE_DIR}/src/tests/api/interface/api_game_operations_test.h"
    "${PROJECT_SOURCE_DIR}/src/tests/api/interface/create_game_handle_test.h"
    "${PROJECT_SOURCE_DIR}/src/tests/api/interface/database_interface_test.h"
    "${PROJECT_SOURCE_DIR}/src/tests/api/interface/game_interface_test.h"
    "${PROJECT_SOURCE_DIR}/src/tests/api/interface/is_compatible_test.h"
    "${PROJECT_SOURCE_DIR}/src/tests/api/interface/metadata/file_test.h"
    "${PROJECT_SOURCE_DIR}/src/tests/api/interface/metadata/group_test.h"
    "${PROJECT_SOURCE_DIR}/src/tests/api/interface/metadata/location_test.h"
    "${PROJECT_SOURCE_DIR}/src/tests/api/interface/metadata/message_test.h"
    "${PROJECT_SOURCE_DIR}/src/tests/api/interface/metadata/message_content_test.h"
    "${PROJECT_SOURCE_DIR}/src/tests/api/interface/metadata/plugin_cleaning_data_test.h"
    "${PROJECT_SOURCE_DIR}/src/tests/api/interface/metadata/plugin_metadata_test.h"
    "${PROJECT_SOURCE_DIR}/src/tests/api/interface/metadata/tag_test.h")

source_group(TREE "${PROJECT_SOURCE_DIR}/src/tests/api/interface"
    PREFIX "Source Files"
    FILES ${LIBLOOT_SRC_TESTS_INTERFACE_CPP_FILES})

source_group(TREE "${PROJECT_SOURCE_DIR}/src/tests/api/interface"
    PREFIX "Header Files"
    FILES ${LIBLOOT_SRC_TESTS_INTERFACE_H_FILES})

set(LIBLOOT_INTERFACE_TESTS_ALL_SOURCES
    ${LIBLOOT_SRC_TESTS_INTERFACE_CPP_FILES}
    ${LIBLOOT_SRC_TESTS_INTERFACE_H_FILES}
    "${PROJECT_SOURCE_DIR}/src/tests/common_game_test_fixture.h"
    "${PROJECT_SOURCE_DIR}/src/tests/test_helpers.h"
    "${PROJECT_SOURCE_DIR}/src/tests/printers.h")

if(MSVC)
    list(APPEND LIBLOOT_INTERFACE_TESTS_ALL_SOURCES "${PROJECT_SOURCE_DIR}/src/tests/libloot_tests.manifest")
endif()

##############################
# Define Targets
##############################

# Build API tests.
add_executable(libloot_tests ${LIBLOOT_INTERFACE_TESTS_ALL_SOURCES})
target_link_libraries(libloot_tests PRIVATE loot GTest::gtest_main)

enable_testing()
gtest_discover_tests(libloot_tests DISCOVERY_TIMEOUT 10)

##############################
# Set Target-Specific Flags
##############################

target_include_directories(libloot_tests PRIVATE ${LIBLOOT_INCLUDE_DIRS})
target_include_directories(libloot_tests SYSTEM PRIVATE
    ${LIBLOOT_COMMON_SYSTEM_INCLUDE_DIRS})

set_target_properties(libloot_tests PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON)

if(WIN32)
    target_compile_definitions(libloot_tests PRIVATE UNICODE _UNICODE)

    if((NOT CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows") OR NOT LIBLOOT_BUILD_SHARED)
        target_compile_definitions(libloot_tests PRIVATE LOOT_STATIC)
    endif()

    target_link_libraries(libloot_tests PRIVATE ${LOOT_LIBS})
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(libloot_tests PRIVATE "-Wall" "-Wextra")
endif()

if(MSVC)
    # Turn off permissive mode to be more standards-compliant and avoid compiler errors.
    target_compile_options(libloot_tests PRIVATE "/Zc:__cplusplus" "/permissive-" "/W4" "/MP")
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

    # Skip some checks for tests because they're not worth the noise (e.g. GTest
    # happens to use goto).
    set(CLANG_TIDY_TEST_CHECKS ${CLANG_TIDY_COMMON_CHECKS})

    list(JOIN CLANG_TIDY_TEST_CHECKS "," CLANG_TIDY_TEST_CHECKS_JOINED)

    set(CLANG_TIDY_TEST
        clang-tidy "-header-filter=.*" "-checks=${CLANG_TIDY_TEST_CHECKS_JOINED}")

    set_target_properties(libloot_tests
        PROPERTIES
            CXX_CLANG_TIDY "${CLANG_TIDY_TEST}")
endif()


##############################
# Post-Build Steps
##############################

# Copy testing plugins
add_custom_command(TARGET libloot_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${testing-plugins_SOURCE_DIR}
        ${PROJECT_BINARY_DIR}/testing-plugins)
