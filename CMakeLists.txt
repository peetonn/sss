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
    
project(serializer C VERSION ${PROJECT_VERSION})

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

# Library
add_library(serializer
    src/serializer.c
    # src/binary_format.c
    # src/json_format.c
)

target_include_directories(serializer
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Examples
add_executable(basic_example examples/basic_example.c)
target_link_libraries(basic_example PRIVATE serializer)

add_executable(proto_example examples/proto_example.c)
target_link_libraries(proto_example PRIVATE serializer)