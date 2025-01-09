include(FetchContent)

cmake_minimum_required(VERSION 3.14)

enable_testing()

# setup unity test framework
FetchContent_Declare(
    unity
    GIT_REPOSITORY https://github.com/ThrowTheSwitch/Unity.git
    GIT_TAG v2.6.0
    EXCLUDE_FROM_ALL
)

FetchContent_GetProperties(unity)

if(NOT unity_POPULATED)
  # compile with UNITY_INCLUDE_CONFIG_H
  add_compile_definitions(UNITY_INCLUDE_CONFIG_H)
  add_compile_definitions(_POSIX_C_SOURCE=200809L)
  FetchContent_Populate(unity)
  # copy unity_config.h to unity
  file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/unity/unity_config.h DESTINATION ${unity_SOURCE_DIR}/src)
  add_subdirectory(${unity_SOURCE_DIR} ${unity_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()

set(COMMON_SRCS 
    ${CMAKE_CURRENT_SOURCE_DIR}/common.c
)
set(COMMON_COMPILE_DEFINITIONS
        UNITY_OUTPUT_COLOR
        UNITY_INCLUDE_CONFIG_H
)
set(COMMON_LINK_LIBRARIES
        unity
        sss
)

# add tests
add_executable(tlv_encode_decode_tests
    ${COMMON_SRCS}
    tlv_encode_decode_tests.c
)
target_compile_definitions(tlv_encode_decode_tests PRIVATE ${COMMON_COMPILE_DEFINITIONS})
target_link_libraries(tlv_encode_decode_tests PRIVATE ${COMMON_LINK_LIBRARIES})
add_test(NAME tlv_encode_decode_tests COMMAND tlv_encode_decode_tests)

add_executable(serialize_tests
    ${COMMON_SRCS}
    serialize_tests.c
)
target_compile_definitions(serialize_tests PRIVATE ${COMMON_COMPILE_DEFINITIONS})
target_link_libraries(serialize_tests PRIVATE ${COMMON_LINK_LIBRARIES})
add_test(NAME serialize_tests COMMAND serialize_tests)