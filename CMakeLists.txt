cmake_minimum_required(VERSION 3.8)
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()
project(libloot)
include(ExternalProject)

option(BUILD_SHARED_LIBS "Build a shared library" ON)
option(RUN_CLANG_TIDY "Whether or not to run clang-tidy during build. Has no effect when using CMake's MSVC generator." OFF)

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
else()
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

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
        if(CMAKE_GENERATOR_PLATFORM STREQUAL  "Win32")
            set(RUST_TARGET i686-pc-windows-msvc)
        else()
            set(RUST_TARGET x86_64-pc-windows-msvc)
        endif()
    else()
        if(CMAKE_GENERATOR_PLATFORM STREQUAL  "Win32")
            set(RUST_TARGET i686-pc-windows-gnu)
        else()
            set(RUST_TARGET x86_64-pc-windows-gnu)
        endif()
    endif()
else()
    set(RUST_TARGET x86_64-unknown-linux-gnu)
endif()

find_package(Boost REQUIRED)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
    find_package(ICU 71.1 REQUIRED COMPONENTS data uc)
    find_package(TBB REQUIRED)
endif()

ExternalProject_Add(GTest
    PREFIX "external"
    URL "https://github.com/google/googletest/archive/v1.14.0.tar.gz"
    URL_HASH "SHA256=8ad598c73ad796e0d8280b082cebd82a630d73e73cd3c70057938a6501bba5d7"
    CMAKE_ARGS
        -Dgtest_force_shared_crt=ON
        -DCMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS}
        -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}
    INSTALL_COMMAND "")
ExternalProject_Get_Property(GTest SOURCE_DIR BINARY_DIR)
set(GTEST_INCLUDE_DIRS "${SOURCE_DIR}/googletest/include")
set(GTEST_LIBRARIES "${BINARY_DIR}/lib/${CMAKE_CFG_INTDIR}/${CMAKE_STATIC_LIBRARY_PREFIX}gtest${CMAKE_STATIC_LIBRARY_SUFFIX}")

ExternalProject_Add(esplugin
    PREFIX "external"
    URL "https://github.com/Ortham/esplugin/archive/4.1.1.tar.gz"
    URL_HASH "SHA256=4c61199fc85ae7ebd8a3954c95e99598b4052ee923567d24f95d7a334c2c6525"
    CONFIGURE_COMMAND ""
    BUILD_IN_SOURCE 1
    BUILD_COMMAND cargo build --release --manifest-path ffi/Cargo.toml --target ${RUST_TARGET} &&
        cbindgen ffi/ -o ffi/include/esplugin.hpp
    INSTALL_COMMAND "")
ExternalProject_Get_Property(esplugin SOURCE_DIR)
set(ESPLUGIN_INCLUDE_DIRS "${SOURCE_DIR}/ffi/include")
set(ESPLUGIN_LIBRARIES "${SOURCE_DIR}/target/${RUST_TARGET}/release/${CMAKE_STATIC_LIBRARY_PREFIX}esplugin_ffi${CMAKE_STATIC_LIBRARY_SUFFIX}")
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(ESPLUGIN_LIBRARIES ${ESPLUGIN_LIBRARIES} userenv ntdll)
else()
    set(ESPLUGIN_LIBRARIES ${ESPLUGIN_LIBRARIES} dl)
endif()

ExternalProject_Add(libloadorder
    PREFIX "external"
    URL "https://github.com/Ortham/libloadorder/archive/15.0.2.tar.gz"
    URL_HASH "SHA256=7048984da19f6e1e6a271b0b396fc5a47ae3c25ff715995c885dc8a7a9bbc5d6"
    CONFIGURE_COMMAND ""
    BUILD_IN_SOURCE 1
    BUILD_COMMAND cargo build --release --manifest-path ffi/Cargo.toml --target ${RUST_TARGET} &&
        cbindgen ffi/ -l c++ -o ffi/include/libloadorder.hpp
    INSTALL_COMMAND "")
ExternalProject_Get_Property(libloadorder SOURCE_DIR)
set(LIBLOADORDER_INCLUDE_DIRS "${SOURCE_DIR}/ffi/include")
set(LIBLOADORDER_LIBRARIES "${SOURCE_DIR}/target/${RUST_TARGET}/release/${CMAKE_STATIC_LIBRARY_PREFIX}loadorder_ffi${CMAKE_STATIC_LIBRARY_SUFFIX}")
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(LIBLOADORDER_LIBRARIES ${LIBLOADORDER_LIBRARIES} userenv)
else()
    set(LIBLOADORDER_LIBRARIES ${LIBLOADORDER_LIBRARIES} dl)
endif()

ExternalProject_Add(loot-condition-interpreter
    PREFIX "external"
    URL "https://github.com/loot/loot-condition-interpreter/archive/3.1.0.tar.gz"
    URL_HASH "SHA256=a2dcadff0e6f9fd4fdc12785d296bf5ee24cc556f4b20a357233e2a2e79d025c"
    CONFIGURE_COMMAND ""
    BUILD_IN_SOURCE 1
    BUILD_COMMAND cargo build --release --manifest-path ffi/Cargo.toml --target ${RUST_TARGET} &&
        cbindgen ffi/ -o ffi/include/loot_condition_interpreter.h
    INSTALL_COMMAND "")
ExternalProject_Get_Property(loot-condition-interpreter SOURCE_DIR)
set(LCI_INCLUDE_DIRS "${SOURCE_DIR}/ffi/include")
set(LCI_LIBRARIES "${SOURCE_DIR}/target/${RUST_TARGET}/release/${CMAKE_STATIC_LIBRARY_PREFIX}loot_condition_interpreter_ffi${CMAKE_STATIC_LIBRARY_SUFFIX}")
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(LCI_LIBRARIES ${LCI_LIBRARIES} userenv)
else()
    set(LCI_LIBRARIES ${LCI_LIBRARIES} dl)
endif()

ExternalProject_Add(testing-plugins
    PREFIX "external"
    URL "https://github.com/Ortham/testing-plugins/archive/1.5.0.tar.gz"
    URL_HASH "SHA256=98c9094fb0f0152b1af7a6206950207f7ddc6602cd44ed67ebf70603ef490791"
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND "")

ExternalProject_Add(spdlog
    PREFIX "external"
    URL "https://github.com/gabime/spdlog/archive/v1.12.0.tar.gz"
    URL_HASH "SHA256=4dccf2d10f410c1e2feaff89966bfc49a1abb29ef6f08246335b110e001e09a9"
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND "")
ExternalProject_Get_Property(spdlog SOURCE_DIR)
set(SPDLOG_INCLUDE_DIRS "${SOURCE_DIR}/include")

ExternalProject_Add(yaml-cpp
    PREFIX "external"
    URL "https://github.com/loot/yaml-cpp/archive/0.8.0+merge-key-support.1.tar.gz"
    URL_HASH "SHA256=07ebe9352aec55f1b2d855c8751befcb981b42d4602336527edcfd1bfae9a1fc"
    CMAKE_ARGS
        -DYAML_MSVC_SHARED_RT=ON
        -DYAML_CPP_BUILD_CONTRIB=OFF
        -DYAML_CPP_BUILD_TOOLS=OFF
        -DYAML_CPP_BUILD_TESTS=OFF
        -DYAML_CPP_INSTALL=OFF
        -DYAML_CPP_FORMAT_SOURCE=OFF
        -DCMAKE_POSITION_INDEPENDENT_CODE=${CMAKE_POSITION_INDEPENDENT_CODE}
        -DCMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS}
        -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}
        -DBUILD_SHARED_LIBS=OFF
    INSTALL_COMMAND "")
ExternalProject_Get_Property(yaml-cpp SOURCE_DIR BINARY_DIR)

set(YAML_CPP_LIBRARY_SUFFIX "$<$<CONFIG:Debug>:d>${CMAKE_STATIC_LIBRARY_SUFFIX}")
set(YAML_CPP_INCLUDE_DIRS "${SOURCE_DIR}/include")
set(YAML_CPP_LIBRARIES "${BINARY_DIR}/$<CONFIG>/${CMAKE_STATIC_LIBRARY_PREFIX}yaml-cpp${YAML_CPP_LIBRARY_SUFFIX}")


##############################
# General Settings
##############################

set(LIBLOOT_SRC_API_CPP_FILES
    "${CMAKE_SOURCE_DIR}/src/api/api.cpp"
    "${CMAKE_SOURCE_DIR}/src/api/api_database.cpp"
    "${CMAKE_SOURCE_DIR}/src/api/bsa.cpp"
    "${CMAKE_SOURCE_DIR}/src/api/error_categories.cpp"
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
    "${CMAKE_SOURCE_DIR}/src/api/sorting/cyclic_interaction_error.cpp"
    "${CMAKE_SOURCE_DIR}/src/api/sorting/group_sort.cpp"
    "${CMAKE_SOURCE_DIR}/src/api/sorting/plugin_sort.cpp"
    "${CMAKE_SOURCE_DIR}/src/api/sorting/plugin_graph.cpp"
    "${CMAKE_SOURCE_DIR}/src/api/sorting/plugin_sorting_data.cpp"
    "${CMAKE_SOURCE_DIR}/src/api/sorting/undefined_group_error.cpp"
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
    "${CMAKE_SOURCE_DIR}/src/api/helpers/collections.h"
    "${CMAKE_SOURCE_DIR}/src/api/helpers/crc.h"
    "${CMAKE_SOURCE_DIR}/src/api/helpers/logging.h"
    "${CMAKE_SOURCE_DIR}/src/api/helpers/text.h")

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
    "${CMAKE_SOURCE_DIR}/src/tests/api/internals/metadata_list_test.h"
)

set(LIBLOOT_SRC_TESTS_INTERFACE_CPP_FILES
"${CMAKE_SOURCE_DIR}/src/tests/api/interface/main.cpp"
)

set(LIBLOOT_SRC_TESTS_INTERFACE_H_FILES
    "${CMAKE_SOURCE_DIR}/src/tests/api/interface/api_game_operations_test.h"
    "${CMAKE_SOURCE_DIR}/src/tests/api/interface/create_game_handle_test.h"
    "${CMAKE_SOURCE_DIR}/src/tests/api/interface/database_interface_test.h"
    "${CMAKE_SOURCE_DIR}/src/tests/api/interface/game_interface_test.h"
    "${CMAKE_SOURCE_DIR}/src/tests/api/interface/is_compatible_test.h")

source_group(TREE "${CMAKE_SOURCE_DIR}/src/api"
    PREFIX "Source Files"
    FILES ${LIBLOOT_SRC_API_CPP_FILES})

source_group(TREE "${CMAKE_SOURCE_DIR}/include"
    PREFIX "Header Files"
    FILES ${LIBLOOT_INCLUDE_H_FILES})

source_group(TREE "${CMAKE_SOURCE_DIR}/src/api"
    PREFIX "Header Files"
    FILES ${LIBLOOT_SRC_API_H_FILES})

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

set(LIBLOOT_ALL_SOURCES
    ${LIBLOOT_SRC_API_CPP_FILES}
    ${LIBLOOT_INCLUDE_H_FILES}
    ${LIBLOOT_SRC_API_H_FILES}
    "${CMAKE_BINARY_DIR}/generated/loot_version.cpp"
    "${CMAKE_SOURCE_DIR}/src/api/resource.rc")

set(LIBLOOT_INTERNALS_TESTS_ALL_SOURCES
    ${LIBLOOT_ALL_SOURCES}
    ${LIBLOOT_SRC_TESTS_INTERNALS_CPP_FILES}
    ${LIBLOOT_SRC_TESTS_INTERNALS_H_FILES}
    "${CMAKE_SOURCE_DIR}/src/tests/common_game_test_fixture.h"
    "${CMAKE_SOURCE_DIR}/src/tests/printers.h"
)

set(LIBLOOT_INTERFACE_TESTS_ALL_SOURCES
    ${LIBLOOT_SRC_TESTS_INTERFACE_CPP_FILES}
    ${LIBLOOT_SRC_TESTS_INTERFACE_H_FILES}
    "${CMAKE_SOURCE_DIR}/src/tests/common_game_test_fixture.h"
    "${CMAKE_SOURCE_DIR}/src/tests/printers.h"
)

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
    yaml-cpp
    GTest
    testing-plugins)
target_link_libraries(libloot_internals_tests PRIVATE
    ${ESPLUGIN_LIBRARIES}
    ${LIBLOADORDER_LIBRARIES}
    ${LCI_LIBRARIES}
    ${YAML_CPP_LIBRARIES}
    ${GTEST_LIBRARIES})

# Build API.
add_library(loot ${LIBLOOT_ALL_SOURCES})
add_dependencies(loot
    esplugin
    libloadorder
    loot-condition-interpreter
    spdlog
    yaml-cpp)
target_link_libraries(loot PRIVATE
    ${ESPLUGIN_LIBRARIES}
    ${LIBLOADORDER_LIBRARIES}
    ${LCI_LIBRARIES}
    ${YAML_CPP_LIBRARIES})

# Build API tests.
add_executable(libloot_tests ${LIBLOOT_INTERFACE_TESTS_ALL_SOURCES})
add_dependencies(libloot_tests loot GTest testing-plugins)
target_link_libraries(libloot_tests PRIVATE loot ${GTEST_LIBRARIES})

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

target_include_directories(loot PRIVATE ${LIBLOOT_INCLUDE_DIRS})
target_include_directories(loot SYSTEM PRIVATE
    ${LIBLOOT_COMMON_SYSTEM_INCLUDE_DIRS})

target_include_directories(libloot_tests PRIVATE ${LIBLOOT_INCLUDE_DIRS})
target_include_directories(libloot_tests SYSTEM PRIVATE
    ${LIBLOOT_COMMON_SYSTEM_INCLUDE_DIRS}
    ${GTEST_INCLUDE_DIRS})

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    target_compile_definitions(libloot_internals_tests PRIVATE
        UNICODE _UNICODE LOOT_STATIC YAML_CPP_STATIC_DEFINE)
    target_compile_definitions(loot PRIVATE
        UNICODE _UNICODE LOOT_EXPORT YAML_CPP_STATIC_DEFINE)
    target_compile_definitions(libloot_tests PRIVATE UNICODE _UNICODE)

    set(LOOT_LIBS ws2_32 bcrypt)

    if(NOT CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
        target_compile_definitions(libloot_tests PRIVATE LOOT_STATIC)
        set(LOOT_LIBS ${LOOT_LIBS} tbb_static)
    endif()

    target_link_libraries(libloot_internals_tests PRIVATE ${LOOT_LIBS})
    target_link_libraries(loot PRIVATE ${LOOT_LIBS})
    target_link_libraries(libloot_tests PRIVATE ${LOOT_LIBS})
else()
    set(LOOT_LIBS ICU::data ICU::uc pthread stdc++fs TBB::tbb)

    target_link_libraries(libloot_internals_tests PRIVATE ${LOOT_LIBS})
    target_link_libraries(loot PRIVATE ${LOOT_LIBS})
    target_link_libraries(libloot_tests PRIVATE ${LOOT_LIBS})
endif()

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set_target_properties(libloot_internals_tests loot libloot_tests
        PROPERTIES
            INSTALL_RPATH "${CMAKE_INSTALL_RPATH};."
            BUILD_WITH_INSTALL_RPATH ON)

    target_compile_options(libloot_internals_tests PRIVATE "-Wall" "-Wextra")
    target_compile_options(loot PRIVATE "-Wall" "-Wextra")
    target_compile_options(libloot_tests PRIVATE "-Wall" "-Wextra")
endif()

if(MSVC)
    # Turn off permissive mode to be more standards-compliant and avoid compiler errors.
    target_compile_options(loot PRIVATE "/permissive-" "/W4")
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

    set(CLANG_TIDY_LIB_CHECKS
        ${CLANG_TIDY_COMMON_CHECKS}
        "cppcoreguidelines-avoid-goto"
        "cppcoreguidelines-avoid-magic-numbers"
        "cppcoreguidelines-non-private-member-variables-in-classes"
        "cppcoreguidelines-special-member-functions")

    # Skip some checks for tests because they're not worth the noise (e.g. GTest
    # happens to use goto).
    set(CLANG_TIDY_TEST_CHECKS ${CLANG_TIDY_COMMON_CHECKS})

    list(JOIN CLANG_TIDY_LIB_CHECKS "," CLANG_TIDY_LIB_CHECKS_JOINED)
    list(JOIN CLANG_TIDY_TEST_CHECKS "," CLANG_TIDY_TEST_CHECKS_JOINED)

    set(CLANG_TIDY_LIB
        clang-tidy "-header-filter=.*" "-checks=${CLANG_TIDY_LIB_CHECKS_JOINED}")

    set(CLANG_TIDY_TEST
        clang-tidy "-header-filter=.*" "-checks=${CLANG_TIDY_TEST_CHECKS_JOINED}")

    set_target_properties(loot
        PROPERTIES
            CXX_CLANG_TIDY "${CLANG_TIDY_LIB}")

    set_target_properties(libloot_internals_tests libloot_tests
        PROPERTIES
            CXX_CLANG_TIDY "${CLANG_TIDY_TEST}")
endif()

##############################
# Post-Build Steps
##############################

# Copy testing plugins
ExternalProject_Get_Property(testing-plugins SOURCE_DIR)
add_custom_command(TARGET libloot_internals_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${SOURCE_DIR}
        $<TARGET_FILE_DIR:libloot_internals_tests>)
add_custom_command(TARGET libloot_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${SOURCE_DIR}
      $<TARGET_FILE_DIR:libloot_tests>)

########################################
# Install
########################################

install(TARGETS loot
    DESTINATION ".")

if(MSVC)
    install(FILES $<TARGET_PDB_FILE:loot>
        DESTINATION .
        OPTIONAL
        CONFIGURATIONS RelWithDebInfo)
endif()

install(DIRECTORY "${CMAKE_SOURCE_DIR}/include"
    DESTINATION ".")

install(DIRECTORY "${CMAKE_BINARY_DIR}/docs/html/"
    DESTINATION "docs")

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
