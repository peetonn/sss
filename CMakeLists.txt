cmake_minimum_required(VERSION 3.14)

# read .version file from current dir
if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/.version)
    file(READ ${CMAKE_CURRENT_SOURCE_DIR}/.version PROJECT_VERSION)

    if (PROJECT_VERSION)
        string(STRIP ${PROJECT_VERSION} PROJECT_VERSION)
    else ()
        message(FATAL_ERROR "Project version file .version is empty")
    endif ()

    message("Current project version is: ${PROJECT_VERSION}")
else()
    message(FATAL_ERROR "Project version file .version not found")
endif ()
    
project(serializer VERSION ${PROJECT_VERSION} DESCRIPTION "SSS - A simple struct serializer" LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)

find_program(CLANG_TIDY_COMMAND NAMES clang-tidy)
if (CLANG_TIDY_COMMAND)
    set(CMAKE_CXX_CLANG_TIDY clang-tidy)
else ()
    message(STATUS "clang-tidy not found; automatic linting will not be performed")
endif ()

# top-level definitions
execute_process(
        COMMAND git rev-parse --abbrev-ref HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE PROJECT_BRANCH
        OUTPUT_STRIP_TRAILING_WHITESPACE
)
execute_process(
        COMMAND git rev-parse --short HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE PROJECT_COMMIT
        OUTPUT_STRIP_TRAILING_WHITESPACE
)
add_definitions(
    -DPROJECT_VERSION="${PROJECT_VERSION}"
    -DPROJECT_FULL_VERSION="${PROJECT_VERSION}-${PROJECT_BRANCH}-${PROJECT_COMMIT}"
    -DPROJECT_BRANCH="${PROJECT_BRANCH}"
    -DPROJECT_COMMIT="${PROJECT_COMMIT}"    
    )
add_compile_definitions("DEBUG=$<CONFIG:Debug>")

# add SSS_DEBUG_LOG for debug builds
if (CMAKE_BUILD_TYPE MATCHES Debug)
    add_compile_definitions(SSS_DEBUG_LOG)
endif ()

# Library
set(LIB_NAME sss)
add_library(${LIB_NAME}
    src/serializer.c
    src/tlv.c
)
set_target_properties(${LIB_NAME} PROPERTIES VERSION ${PROJECT_VERSION})
set_target_properties(${LIB_NAME} PROPERTIES PUBLIC_HEADER "include/sss/sss.h")
target_include_directories(${LIB_NAME}
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<INSTALL_INTERFACE:include>"
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)


# Install
install(TARGETS ${LIB_NAME}
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include/${LIB_NAME}
    PUBLIC_HEADER DESTINATION include/${LIB_NAME}
)

# Tests
if (BUILD_TESTING)
    set(SSS_BUILD_TESTS_default ON)
else()
    set(SSS_BUILD_TESTS_default OFF)
endif()

option(SSS_BUILD_TESTS "Build tests" ${SSS_BUILD_TESTS_default})

if (SSS_BUILD_TESTS)
    add_subdirectory(tests)
endif()


# Examples
option(SSS_BUILD_EXAMPLES "Build examples" OFF)

if (SSS_BUILD_EXAMPLES)
    add_executable(basic_example examples/basic_example.c)
    target_link_libraries(basic_example PRIVATE ${LIB_NAME})

    add_executable(proto_example examples/proto_example.cpp)
    set_target_properties(proto_example PROPERTIES LANGUAGE CXX)
    target_link_libraries(proto_example PRIVATE ${LIB_NAME})
endif ()