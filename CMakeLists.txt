cmake_minimum_required(VERSION 3.8)
cmake_policy(SET CMP0015 NEW)
project(libloot)
include(ExternalProject)

option(BUILD_SHARED_LIBS "Build a shared library" ON)

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
    if(CMAKE_GENERATOR_PLATFORM STREQUAL  "Win32")
        set(RUST_TARGET i686-pc-windows-msvc)
    else()
        set(RUST_TARGET x86_64-pc-windows-msvc)
    endif()
else()
    set(RUST_TARGET x86_64-unknown-linux-gnu)
endif()

find_package(Boost REQUIRED)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
    find_package(ICU REQUIRED COMPONENTS uc)
endif()

ExternalProject_Add(GTest
    PREFIX "external"
    URL "https://github.com/google/googletest/archive/release-1.11.0.tar.gz"
    CMAKE_ARGS -Dgtest_force_shared_crt=ON -DCMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS}
    INSTALL_COMMAND "")
ExternalProject_Get_Property(GTest SOURCE_DIR BINARY_DIR)
set(GTEST_INCLUDE_DIRS "${SOURCE_DIR}/googletest/include")
set(GTEST_LIBRARIES "${BINARY_DIR}/lib/${CMAKE_CFG_INTDIR}/${CMAKE_STATIC_LIBRARY_PREFIX}gtest${CMAKE_STATIC_LIBRARY_SUFFIX}")

ExternalProject_Add(esplugin
    PREFIX "external"
    URL "https://github.com/Ortham/esplugin/archive/3.5.0.tar.gz"
    CONFIGURE_COMMAND ""
    BUILD_IN_SOURCE 1
    BUILD_COMMAND cargo build --release --manifest-path ffi/Cargo.toml --target ${RUST_TARGET} &&
        cbindgen ffi/ -o ffi/include/esplugin.hpp
    INSTALL_COMMAND "")
ExternalProject_Get_Property(esplugin SOURCE_DIR)
set(ESPLUGIN_INCLUDE_DIRS "${SOURCE_DIR}/ffi/include")
set(ESPLUGIN_LIBRARIES "${SOURCE_DIR}/target/${RUST_TARGET}/release/${CMAKE_STATIC_LIBRARY_PREFIX}esplugin_ffi${CMAKE_STATIC_LIBRARY_SUFFIX}")
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(ESPLUGIN_LIBRARIES ${ESPLUGIN_LIBRARIES} Userenv)
else()
    set(ESPLUGIN_LIBRARIES ${ESPLUGIN_LIBRARIES} dl)
endif()

ExternalProject_Add(libloadorder
    PREFIX "external"
    URL "https://github.com/Ortham/libloadorder/archive/13.0.0.tar.gz"
    CONFIGURE_COMMAND ""
    BUILD_IN_SOURCE 1
    BUILD_COMMAND cargo build --release --manifest-path ffi/Cargo.toml --target ${RUST_TARGET} &&
        cbindgen ffi/ -l c++ -o ffi/include/libloadorder.hpp
    INSTALL_COMMAND "")
ExternalProject_Get_Property(libloadorder SOURCE_DIR)
set(LIBLOADORDER_INCLUDE_DIRS "${SOURCE_DIR}/ffi/include")
set(LIBLOADORDER_LIBRARIES "${SOURCE_DIR}/target/${RUST_TARGET}/release/${CMAKE_STATIC_LIBRARY_PREFIX}loadorder_ffi${CMAKE_STATIC_LIBRARY_SUFFIX}")
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(LIBLOADORDER_LIBRARIES ${LIBLOADORDER_LIBRARIES} Userenv)
else()
    set(LIBLOADORDER_LIBRARIES ${LIBLOADORDER_LIBRARIES} dl)
endif()

ExternalProject_Add(loot-condition-interpreter
    PREFIX "external"
    URL "https://github.com/loot/loot-condition-interpreter/archive/2.2.2.tar.gz"
    CONFIGURE_COMMAND ""
    BUILD_IN_SOURCE 1
    BUILD_COMMAND cargo build --release --manifest-path ffi/Cargo.toml --target ${RUST_TARGET} &&
        cbindgen ffi/ -o ffi/include/loot_condition_interpreter.h
    INSTALL_COMMAND "")
ExternalProject_Get_Property(loot-condition-interpreter SOURCE_DIR)
set(LCI_INCLUDE_DIRS "${SOURCE_DIR}/ffi/include")
set(LCI_LIBRARIES "${SOURCE_DIR}/target/${RUST_TARGET}/release/${CMAKE_STATIC_LIBRARY_PREFIX}loot_condition_interpreter_ffi${CMAKE_STATIC_LIBRARY_SUFFIX}")
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(LCI_LIBRARIES ${LCI_LIBRARIES} Userenv)
else()
    set(LCI_LIBRARIES ${LCI_LIBRARIES} dl)
endif()

ExternalProject_Add(testing-metadata
    PREFIX "external"
    GIT_REPOSITORY "https://github.com/loot/testing-metadata"
    GIT_TAG "1.4.0"
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND "")

ExternalProject_Add(testing-plugins
    PREFIX "external"
    URL "https://github.com/Ortham/testing-plugins/archive/1.4.1.tar.gz"
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND "")

ExternalProject_Add(spdlog
    PREFIX "external"
    URL "https://github.com/gabime/spdlog/archive/v1.9.2.tar.gz"
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND "")
ExternalProject_Get_Property(spdlog SOURCE_DIR)
set(SPDLOG_INCLUDE_DIRS "${SOURCE_DIR}/include")


if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(YAMLCPP_BUILD_COMMAND ${CMAKE_COMMAND} --build . --target yaml-cpp --config $(CONFIGURATION))
else()
    set(YAMLCPP_BUILD_COMMAND ${CMAKE_COMMAND} --build . --target yaml-cpp)
endif()

ExternalProject_Add(yaml-cpp
    PREFIX "external"
    URL "https://github.com/loot/yaml-cpp/archive/yaml-cpp-0.7.0+merge-key-support.1.tar.gz"
    CMAKE_ARGS
        -DYAML_MSVC_SHARED_RT=ON
        -DCMAKE_POSITION_INDEPENDENT_CODE=${CMAKE_POSITION_INDEPENDENT_CODE}
        -DCMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS}
        -DYAML_BUILD_SHARED_LIBS=OFF
    BUILD_COMMAND ${YAMLCPP_BUILD_COMMAND}
    INSTALL_COMMAND "")
ExternalProject_Get_Property(yaml-cpp SOURCE_DIR BINARY_DIR)

set(YAML_CPP_LIBRARY_SUFFIX "$<$<CONFIG:Debug>:d>${CMAKE_STATIC_LIBRARY_SUFFIX}")
set(YAML_CPP_INCLUDE_DIRS "${SOURCE_DIR}/include")
set(YAML_CPP_LIBRARIES "${BINARY_DIR}/${CMAKE_CFG_INTDIR}/${CMAKE_STATIC_LIBRARY_PREFIX}yaml-cpp${YAML_CPP_LIBRARY_SUFFIX}")


##############################
# General Settings
##############################

set(LIBLOOT_SRC_API_CPP_FILES
    "${CMAKE_SOURCE_DIR}/src/api/api.cpp"
    "${CMAKE_SOURCE_DIR}/src/api/api_database.cpp"
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
    "${CMAKE_SOURCE_DIR}/src/api/helpers/crc.cpp"
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
    "${CMAKE_SOURCE_DIR}/include/loot/struct/file_revision.h"
    "${CMAKE_SOURCE_DIR}/include/loot/struct/simple_message.h"
    "${CMAKE_SOURCE_DIR}/include/loot/vertex.h")

set(LIBLOOT_SRC_API_H_FILES
    "${CMAKE_SOURCE_DIR}/src/api/api_database.h"
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
    testing-metadata
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
add_dependencies(libloot_tests loot GTest testing-metadata testing-plugins)
target_link_libraries(libloot_tests PRIVATE loot ${GTEST_LIBRARIES})

##############################
# Set Target-Specific Flags
##############################

set(LIBLOOT_COMMON_INCLUDE_DIRS
    "${CMAKE_SOURCE_DIR}/src"
    "${CMAKE_SOURCE_DIR}/include"
    ${LIBLOADORDER_INCLUDE_DIRS}
    ${ESPLUGIN_INCLUDE_DIRS}
    ${LCI_INCLUDE_DIRS}
    ${Boost_INCLUDE_DIRS}
    ${ICU_INCLUDE_DIRS}
    ${SPDLOG_INCLUDE_DIRS}
    ${YAML_CPP_INCLUDE_DIRS})

target_include_directories(libloot_internals_tests PRIVATE
    ${LIBLOOT_COMMON_INCLUDE_DIRS}
    ${GTEST_INCLUDE_DIRS})

target_include_directories(loot PRIVATE ${LIBLOOT_COMMON_INCLUDE_DIRS})

target_include_directories(libloot_tests PRIVATE
    ${LIBLOOT_COMMON_INCLUDE_DIRS}
    ${GTEST_INCLUDE_DIRS})

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    target_compile_definitions(libloot_internals_tests PRIVATE
        UNICODE _UNICODE LOOT_STATIC)
    target_compile_definitions(loot PRIVATE UNICODE _UNICODE LOOT_EXPORT)
    target_compile_definitions(libloot_tests PRIVATE UNICODE _UNICODE)
else()
    target_link_libraries(libloot_internals_tests PRIVATE ICU::uc)
    target_link_libraries(loot PRIVATE ICU::uc)
endif()

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set_target_properties(libloot_internals_tests loot libloot_tests
        PROPERTIES
            INSTALL_RPATH "${CMAKE_INSTALL_RPATH};."
            BUILD_WITH_INSTALL_RPATH ON)

    set(LOOT_LIBS ssl
        curl
        z
        crypto
        rt
        pthread
        icuuc
        icui18n
        ssh2
        http_parser
        stdc++fs)

    if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        list(APPEND LOOT_LIBS supc++)
    endif()

    target_link_libraries(libloot_internals_tests PRIVATE ${LOOT_LIBS})
    target_link_libraries(loot PRIVATE ${LOOT_LIBS})
    target_link_libraries(libloot_tests PRIVATE pthread)
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

    set(LOOT_LIBS
        version
        ws2_32
        shlwapi
        winhttp
        crypt32
        Rpcrt4
        bcrypt)

    target_link_libraries(libloot_internals_tests PRIVATE ${LOOT_LIBS})
    target_link_libraries(loot PRIVATE ${LOOT_LIBS})
endif()

##############################
# Post-Build Steps
##############################

# Copy testing metadata
ExternalProject_Get_Property(testing-metadata SOURCE_DIR)
add_custom_command(TARGET libloot_internals_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${SOURCE_DIR}
        "$<TARGET_FILE_DIR:libloot_internals_tests>/testing-metadata")
add_custom_command(TARGET libloot_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${SOURCE_DIR}
        "$<TARGET_FILE_DIR:libloot_tests>/testing-metadata")

if(GIT_FOUND)
	# Clone a mirror of the testing-metadata repository for faster tests.
    # Remove the directory if it already exists before cloning, as the clone
    # will fail if the target directory exists and is not empty.
	# Tests will fail if this is not done.

	add_custom_command(TARGET libloot_internals_tests POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E remove_directory
            "$<TARGET_FILE_DIR:libloot_internals_tests>/testing-metadata.git")

	add_custom_command(TARGET libloot_internals_tests POST_BUILD
		COMMAND git clone --bare https://github.com/loot/testing-metadata.git "$<TARGET_FILE_DIR:libloot_internals_tests>/testing-metadata.git")

	add_custom_command(TARGET libloot_tests POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E remove_directory
            "$<TARGET_FILE_DIR:libloot_tests>/testing-metadata.git")

	add_custom_command(TARGET libloot_tests POST_BUILD
		COMMAND git clone --bare https://github.com/loot/testing-metadata.git "$<TARGET_FILE_DIR:libloot_tests>/testing-metadata.git")
endif()

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
