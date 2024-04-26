##############################
# Dependencies
##############################

include(FetchContent)

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
    URL "https://github.com/google/googletest/archive/refs/tags/v1.14.0.tar.gz"
    URL_HASH "SHA256=8ad598c73ad796e0d8280b082cebd82a630d73e73cd3c70057938a6501bba5d7"
    FIND_PACKAGE_ARGS)

FetchContent_Declare(
    testing-plugins
    URL "https://github.com/Ortham/testing-plugins/archive/1.5.0.tar.gz"
    URL_HASH "SHA256=98c9094fb0f0152b1af7a6206950207f7ddc6602cd44ed67ebf70603ef490791")

FetchContent_MakeAvailable(GTest testing-plugins)

set(BUILD_SHARED_LIBS ${BUILD_SHARED_LIBS_INITIAL})


##############################
# General Settings
##############################

set(LIBLOOT_SRC_TESTS_INTERNALS_CPP_FILES
    "${CMAKE_SOURCE_DIR}/src/tests/api/internals/main.cpp")

set(LIBLOOT_SRC_TESTS_INTERNALS_H_FILES
    "${CMAKE_SOURCE_DIR}/src/tests/api/internals/game/game_test.h"
    "${CMAKE_SOURCE_DIR}/src/tests/api/internals/game/game_cache_test.h"
    "${CMAKE_SOURCE_DIR}/src/tests/api/internals/game/load_order_handler_test.h"
    "${CMAKE_SOURCE_DIR}/src/tests/api/internals/helpers/crc_test.h"
    "${CMAKE_SOURCE_DIR}/src/tests/api/internals/helpers/text_test.h"
    "${CMAKE_SOURCE_DIR}/src/tests/api/internals/helpers/yaml_set_helpers_test.h"
    "${CMAKE_SOURCE_DIR}/src/tests/api/internals/metadata/condition_evaluator_test.h"
    "${CMAKE_SOURCE_DIR}/src/tests/api/internals/metadata/conditional_metadata_test.h"
    "${CMAKE_SOURCE_DIR}/src/tests/api/internals/metadata/file_test.h"
    "${CMAKE_SOURCE_DIR}/src/tests/api/internals/metadata/group_test.h"
    "${CMAKE_SOURCE_DIR}/src/tests/api/internals/metadata/location_test.h"
    "${CMAKE_SOURCE_DIR}/src/tests/api/internals/metadata/message_test.h"
    "${CMAKE_SOURCE_DIR}/src/tests/api/internals/metadata/message_content_test.h"
    "${CMAKE_SOURCE_DIR}/src/tests/api/internals/metadata/plugin_cleaning_data_test.h"
    "${CMAKE_SOURCE_DIR}/src/tests/api/internals/metadata/plugin_metadata_test.h"
    "${CMAKE_SOURCE_DIR}/src/tests/api/internals/metadata/tag_test.h"
    "${CMAKE_SOURCE_DIR}/src/tests/api/internals/plugin_test.h"
    "${CMAKE_SOURCE_DIR}/src/tests/api/internals/sorting/group_sort_test.h"
    "${CMAKE_SOURCE_DIR}/src/tests/api/internals/sorting/plugin_sort_test.h"
    "${CMAKE_SOURCE_DIR}/src/tests/api/internals/sorting/plugin_graph_test.h"
    "${CMAKE_SOURCE_DIR}/src/tests/api/internals/sorting/plugin_sorting_data_test.h"
    "${CMAKE_SOURCE_DIR}/src/tests/api/internals/metadata_list_test.h")

set(LIBLOOT_SRC_TESTS_INTERFACE_CPP_FILES
    "${CMAKE_SOURCE_DIR}/src/tests/api/interface/main.cpp")

set(LIBLOOT_SRC_TESTS_INTERFACE_H_FILES
    "${CMAKE_SOURCE_DIR}/src/tests/api/interface/api_game_operations_test.h"
    "${CMAKE_SOURCE_DIR}/src/tests/api/interface/create_game_handle_test.h"
    "${CMAKE_SOURCE_DIR}/src/tests/api/interface/database_interface_test.h"
    "${CMAKE_SOURCE_DIR}/src/tests/api/interface/game_interface_test.h"
    "${CMAKE_SOURCE_DIR}/src/tests/api/interface/is_compatible_test.h")

source_group(TREE "${CMAKE_SOURCE_DIR}/src/tests/api/internals"
    PREFIX "Source Files"
    FILES ${LIBLOOT_SRC_TESTS_INTERNALS_CPP_FILES})

source_group(TREE "${CMAKE_SOURCE_DIR}/src/tests/api/internals"
    PREFIX "Header Files"
    FILES ${LIBLOOT_SRC_TESTS_INTERNALS_H_FILES})

source_group(TREE "${CMAKE_SOURCE_DIR}/src/tests/api/interface"
    PREFIX "Source Files"
    FILES ${LIBLOOT_SRC_TESTS_INTERFACE_CPP_FILES})

source_group(TREE "${CMAKE_SOURCE_DIR}/src/tests/api/interface"
    PREFIX "Header Files"
    FILES ${LIBLOOT_SRC_TESTS_INTERFACE_H_FILES})


set(LIBLOOT_INTERNALS_TESTS_ALL_SOURCES
    ${LIBLOOT_ALL_SOURCES}
    ${LIBLOOT_SRC_TESTS_INTERNALS_CPP_FILES}
    ${LIBLOOT_SRC_TESTS_INTERNALS_H_FILES}
    "${CMAKE_SOURCE_DIR}/src/tests/common_game_test_fixture.h"
    "${CMAKE_SOURCE_DIR}/src/tests/printers.h")

set(LIBLOOT_INTERFACE_TESTS_ALL_SOURCES
    ${LIBLOOT_SRC_TESTS_INTERFACE_CPP_FILES}
    ${LIBLOOT_SRC_TESTS_INTERFACE_H_FILES}
    "${CMAKE_SOURCE_DIR}/src/tests/common_game_test_fixture.h"
    "${CMAKE_SOURCE_DIR}/src/tests/printers.h")


##############################
# Define Targets
##############################

# Build tests.
add_executable(libloot_internals_tests ${LIBLOOT_INTERNALS_TESTS_ALL_SOURCES})
add_dependencies(libloot_internals_tests
    esplugin
    libloadorder
    loot-condition-interpreter
    spdlog
    yaml-cpp)
target_link_libraries(libloot_internals_tests PRIVATE
    ${ESPLUGIN_LIBRARIES}
    ${LIBLOADORDER_LIBRARIES}
    ${LCI_LIBRARIES}
    ${YAML_CPP_LIBRARIES}
    GTest::gtest_main)

# Build API tests.
add_executable(libloot_tests ${LIBLOOT_INTERFACE_TESTS_ALL_SOURCES})
add_dependencies(libloot_tests loot)
target_link_libraries(libloot_tests PRIVATE loot GTest::gtest_main)


##############################
# Set Target-Specific Flags
##############################

set(LIBLOOT_INCLUDE_DIRS
"${CMAKE_SOURCE_DIR}/src"
"${CMAKE_SOURCE_DIR}/include")

set(LIBLOOT_COMMON_SYSTEM_INCLUDE_DIRS
${LIBLOADORDER_INCLUDE_DIRS}
${ESPLUGIN_INCLUDE_DIRS}
${LCI_INCLUDE_DIRS}
${Boost_INCLUDE_DIRS}
${ICU_INCLUDE_DIRS}
${SPDLOG_INCLUDE_DIRS}
${YAML_CPP_INCLUDE_DIRS})

target_include_directories(libloot_internals_tests PRIVATE
${LIBLOOT_INCLUDE_DIRS})

target_include_directories(libloot_internals_tests SYSTEM PRIVATE
${LIBLOOT_COMMON_SYSTEM_INCLUDE_DIRS}
${GTEST_INCLUDE_DIRS})

target_include_directories(libloot_tests PRIVATE ${LIBLOOT_INCLUDE_DIRS})
target_include_directories(libloot_tests SYSTEM PRIVATE
    ${LIBLOOT_COMMON_SYSTEM_INCLUDE_DIRS}
    ${GTEST_INCLUDE_DIRS})

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    target_compile_definitions(libloot_internals_tests PRIVATE
        UNICODE _UNICODE LOOT_STATIC YAML_CPP_STATIC_DEFINE)
    target_compile_definitions(libloot_tests PRIVATE UNICODE _UNICODE)

    set(LOOT_LIBS ws2_32 bcrypt)

    if(NOT CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
        target_compile_definitions(libloot_tests PRIVATE LOOT_STATIC)
        set(LOOT_LIBS ${LOOT_LIBS} tbb_static)
    endif()

    target_link_libraries(libloot_internals_tests PRIVATE ${LOOT_LIBS})
    target_link_libraries(libloot_tests PRIVATE ${LOOT_LIBS})
else()
    set(LOOT_LIBS ICU::data ICU::uc pthread stdc++fs TBB::tbb)

    target_link_libraries(libloot_internals_tests PRIVATE ${LOOT_LIBS})
    target_link_libraries(libloot_tests PRIVATE ${LOOT_LIBS})
endif()

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set_target_properties(libloot_internals_tests libloot_tests
        PROPERTIES
            INSTALL_RPATH "${CMAKE_INSTALL_RPATH};."
            BUILD_WITH_INSTALL_RPATH ON)

    target_compile_options(libloot_internals_tests PRIVATE "-Wall" "-Wextra")
    target_compile_options(libloot_tests PRIVATE "-Wall" "-Wextra")
endif()

if(MSVC)
    # Turn off permissive mode to be more standards-compliant and avoid compiler errors.
    target_compile_options(libloot_tests PRIVATE "/permissive-" "/W4")

    # Set /bigobj to allow building Debug and RelWithDebInfo tests
    target_compile_options(libloot_internals_tests PRIVATE
        "/permissive-"
        "/W4"
        "$<$<OR:$<CONFIG:DEBUG>,$<CONFIG:RelWithDebInfo>>:/bigobj>")
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

    set_target_properties(libloot_internals_tests libloot_tests
        PROPERTIES
            CXX_CLANG_TIDY "${CLANG_TIDY_TEST}")
endif()


##############################
# Post-Build Steps
##############################

# Copy testing plugins
add_custom_command(TARGET libloot_internals_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${testing-plugins_SOURCE_DIR}
        $<TARGET_FILE_DIR:libloot_internals_tests>)

add_custom_command(TARGET libloot_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${testing-plugins_SOURCE_DIR}
        $<TARGET_FILE_DIR:libloot_tests>)
