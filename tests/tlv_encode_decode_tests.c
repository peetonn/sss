/*
 * Created on Wed Jan 08 2025
 *
 * Author: Peter Gusev
 * Copyright (c) 2025 Peter Gusev. All rights reserved.
 */

#include "common.h"

#include <sss/sss.h>
#include <sss/tlv.h>
// unity
#include <unity.h>

struct decode_data {
    int tlv_el_num;
    s_tlv_decoded_element_data tlv_els[1024];
};

void on_tlv_decode_element(const s_tlv_decoded_element_data* tlv_el,
                           void* user_data) {
    struct decode_data* data = (struct decode_data*) user_data;
    data->tlv_els[data->tlv_el_num++] = *tlv_el;
}

void test_tlv_encode_decode_simple_struct() {
    simple_struct ss = {
        .id = 42,
        .value = 3.14f,
        .active = true,
        .name = "Hello, World!",
        .passport_number = "1234567890",
        .blob = {0x01, 0x02, 0x03, 0x04},
    };

    uint8_t buffer[1024];
    size_t bytes_written = 0;

    const s_type_info* info = S_GET_STRUCT_TYPE_INFO(simple_struct);
    s_serializer_error err =
        s_tlv_encode(info, &ss, buffer, sizeof(buffer), &bytes_written);

    TEST_ASSERT_EQUAL(SERIALIZER_OK, err);
    TEST_ASSERT_EQUAL(102, bytes_written);

    struct decode_data data = {0};

    err = s_tlv_decode(buffer, bytes_written, on_tlv_decode_element, &data);

    TEST_ASSERT_EQUAL(SERIALIZER_OK, err);
    TEST_ASSERT_EQUAL(6, data.tlv_el_num);

    { // check each decoded element
        TEST_ASSERT_EQUAL_INT(0, data.tlv_els[0].idx);
        TEST_ASSERT_EQUAL_INT(0, data.tlv_els[0].level);
        TEST_ASSERT_EQUAL_INT(42, *(int*) data.tlv_els[0].value);

        TEST_ASSERT_EQUAL_INT(1, data.tlv_els[1].idx);
        TEST_ASSERT_EQUAL_INT(0, data.tlv_els[1].level);
        TEST_ASSERT_EQUAL_FLOAT(3.14f, *(float*) data.tlv_els[1].value);

        TEST_ASSERT_EQUAL_INT(2, data.tlv_els[2].idx);
        TEST_ASSERT_EQUAL_INT(0, data.tlv_els[2].level);
        TEST_ASSERT_EQUAL_INT(1, *(bool*) data.tlv_els[2].value);

        TEST_ASSERT_EQUAL_INT(3, data.tlv_els[3].idx);
        TEST_ASSERT_EQUAL_INT(0, data.tlv_els[3].level);
        TEST_ASSERT_EQUAL_STRING("Hello, World!",
                                 (const char*) data.tlv_els[3].value);

        TEST_ASSERT_EQUAL_INT(4, data.tlv_els[4].idx);
        TEST_ASSERT_EQUAL_INT(0, data.tlv_els[4].level);
        TEST_ASSERT_EQUAL_STRING("1234567890",
                                 (const char*) data.tlv_els[4].value);

        TEST_ASSERT_EQUAL_INT(5, data.tlv_els[5].idx);
        TEST_ASSERT_EQUAL_INT(0, data.tlv_els[5].level);
        TEST_ASSERT_EQUAL_MEMORY(ss.blob, data.tlv_els[5].value,
                                 sizeof(ss.blob));
    }
}

void test_tlv_encode_decode_nested_struct() {
    nested_struct ns = {
        .id = ENUM_VALUE_2,
        .sub =
            {
                .id = 42,
                .value = 3.14f,
                .active = true,
                .name = "Hello, World!",
                .passport_number = "1234567890",
                .blob = {0x01, 0x02, 0x03, 0x04},
            },
        .name = "Hello, World2!",
    };

    uint8_t buffer[1024];
    size_t bytes_written = 0;

    const s_type_info* info = S_GET_STRUCT_TYPE_INFO(nested_struct);
    s_serializer_error err =
        s_tlv_encode(info, &ns, buffer, sizeof(buffer), &bytes_written);

    TEST_ASSERT_EQUAL(SERIALIZER_OK, err);
    TEST_ASSERT_EQUAL(139, bytes_written);

    struct decode_data data = {0};

    err = s_tlv_decode(buffer, bytes_written, on_tlv_decode_element, &data);

    TEST_ASSERT_EQUAL(SERIALIZER_OK, err);
    TEST_ASSERT_EQUAL(8, data.tlv_el_num);

    { // check each decoded element
        TEST_ASSERT_EQUAL_INT(0, data.tlv_els[0].idx);
        TEST_ASSERT_EQUAL_INT(0, data.tlv_els[0].level);
        TEST_ASSERT_EQUAL_INT(ENUM_VALUE_2, *(int*) data.tlv_els[0].value);

        TEST_ASSERT_EQUAL_INT(0, data.tlv_els[1].idx);
        TEST_ASSERT_EQUAL_INT(1, data.tlv_els[1].level);
        TEST_ASSERT_EQUAL_INT(42, *(int*) data.tlv_els[1].value);

        TEST_ASSERT_EQUAL_INT(1, data.tlv_els[2].idx);
        TEST_ASSERT_EQUAL_INT(1, data.tlv_els[2].level);
        TEST_ASSERT_EQUAL_FLOAT(3.14f, *(float*) data.tlv_els[2].value);

        TEST_ASSERT_EQUAL_INT(2, data.tlv_els[3].idx);
        TEST_ASSERT_EQUAL_INT(1, data.tlv_els[3].level);
        TEST_ASSERT_EQUAL_INT(1, *(bool*) data.tlv_els[3].value);

        TEST_ASSERT_EQUAL_INT(3, data.tlv_els[4].idx);
        TEST_ASSERT_EQUAL_INT(1, data.tlv_els[4].level);
        TEST_ASSERT_EQUAL_STRING("Hello, World!",
                                 (const char*) data.tlv_els[4].value);

        TEST_ASSERT_EQUAL_INT(4, data.tlv_els[5].idx);
        TEST_ASSERT_EQUAL_INT(1, data.tlv_els[5].level);
        TEST_ASSERT_EQUAL_STRING("1234567890",
                                 (const char*) data.tlv_els[5].value);

        TEST_ASSERT_EQUAL_INT(5, data.tlv_els[6].idx);
        TEST_ASSERT_EQUAL_INT(1, data.tlv_els[6].level);
        TEST_ASSERT_EQUAL_MEMORY(ns.sub.blob, data.tlv_els[6].value,
                                 sizeof(ns.sub.blob));

        TEST_ASSERT_EQUAL_INT(2, data.tlv_els[7].idx);
        TEST_ASSERT_EQUAL_INT(0, data.tlv_els[7].level);
        TEST_ASSERT_EQUAL_STRING("Hello, World2!",
                                 (const char*) data.tlv_els[7].value);
    }
}

void test_tlv_encode_decode_nested_union_struct() {
    {
        nested_union_struct ns = {
            .id = ENUM_VALUE_1,
            .data =
                {
                    .sub =
                        {
                            .id = 42,
                            .value = 3.14f,
                            .active = true,
                            .name = "Hello, World!",
                            .passport_number = "1234567890",
                            .blob = {0x01, 0x02, 0x03, 0x04},
                        },
                },
        };

        uint8_t buffer[1024];
        size_t bytes_written = 0;

        const s_type_info* info = S_GET_STRUCT_TYPE_INFO(nested_union_struct);
        s_serializer_error err =
            s_tlv_encode(info, &ns, buffer, sizeof(buffer), &bytes_written);

        TEST_ASSERT_EQUAL(SERIALIZER_OK, err);
        TEST_ASSERT_EQUAL(118, bytes_written);

        struct decode_data data = {0};

        err = s_tlv_decode(buffer, bytes_written, on_tlv_decode_element, &data);

        TEST_ASSERT_EQUAL(SERIALIZER_OK, err);
        TEST_ASSERT_EQUAL(7, data.tlv_el_num);

        { // check each decoded element
            TEST_ASSERT_EQUAL_INT(0, data.tlv_els[0].idx);
            TEST_ASSERT_EQUAL_INT(0, data.tlv_els[0].level);
            TEST_ASSERT_EQUAL_INT(ENUM_VALUE_1, *(int*) data.tlv_els[0].value);

            TEST_ASSERT_EQUAL_INT(0, data.tlv_els[1].idx);
            TEST_ASSERT_EQUAL_INT(1, data.tlv_els[1].level);
            TEST_ASSERT_EQUAL_INT(42, *(int*) data.tlv_els[1].value);

            TEST_ASSERT_EQUAL_INT(1, data.tlv_els[2].idx);
            TEST_ASSERT_EQUAL_INT(1, data.tlv_els[2].level);
            TEST_ASSERT_EQUAL_FLOAT(3.14f, *(float*) data.tlv_els[2].value);

            TEST_ASSERT_EQUAL_INT(2, data.tlv_els[3].idx);
            TEST_ASSERT_EQUAL_INT(1, data.tlv_els[3].level);
            TEST_ASSERT_EQUAL_INT(1, *(bool*) data.tlv_els[3].value);

            TEST_ASSERT_EQUAL_INT(3, data.tlv_els[4].idx);
            TEST_ASSERT_EQUAL_INT(1, data.tlv_els[4].level);
            TEST_ASSERT_EQUAL_STRING("Hello, World!",
                                     (const char*) data.tlv_els[4].value);

            TEST_ASSERT_EQUAL_INT(4, data.tlv_els[5].idx);
            TEST_ASSERT_EQUAL_INT(1, data.tlv_els[5].level);
            TEST_ASSERT_EQUAL_STRING("1234567890",
                                     (const char*) data.tlv_els[5].value);

            TEST_ASSERT_EQUAL_INT(5, data.tlv_els[6].idx);
            TEST_ASSERT_EQUAL_INT(1, data.tlv_els[6].level);
            TEST_ASSERT_EQUAL_MEMORY(ns.data.sub.blob, data.tlv_els[6].value,
                                     sizeof(ns.data.sub.blob));
        }
    }
    {
        nested_union_struct ns = {
            .id = ENUM_VALUE_2,
            .data =
                {
                    .value = 42,
                },
        };

        uint8_t buffer[1024];
        size_t bytes_written = 0;

        const s_type_info* info = S_GET_STRUCT_TYPE_INFO(nested_union_struct);
        s_serializer_error err =
            s_tlv_encode(info, &ns, buffer, sizeof(buffer), &bytes_written);

        TEST_ASSERT_EQUAL(SERIALIZER_OK, err);
        TEST_ASSERT_EQUAL(20, bytes_written);

        struct decode_data data = {0};

        err = s_tlv_decode(buffer, bytes_written, on_tlv_decode_element, &data);

        TEST_ASSERT_EQUAL(SERIALIZER_OK, err);
        TEST_ASSERT_EQUAL(2, data.tlv_el_num);

        { // check each decoded element
            TEST_ASSERT_EQUAL_INT(0, data.tlv_els[0].idx);
            TEST_ASSERT_EQUAL_INT(0, data.tlv_els[0].level);
            TEST_ASSERT_EQUAL_INT(ENUM_VALUE_2, *(int*) data.tlv_els[0].value);

            TEST_ASSERT_EQUAL_INT(1, data.tlv_els[1].idx);
            TEST_ASSERT_EQUAL_INT(0, data.tlv_els[1].level);
            TEST_ASSERT_EQUAL_INT(42, *(int*) data.tlv_els[1].value);
        }
    }
    {
        nested_union_struct ns = {
            .id = ENUM_VALUE_3,
            .data =
                {
                    .name = "Hello, World!",
                },
        };

        uint8_t buffer[1024];
        size_t bytes_written = 0;

        const s_type_info* info = S_GET_STRUCT_TYPE_INFO(nested_union_struct);
        s_serializer_error err =
            s_tlv_encode(info, &ns, buffer, sizeof(buffer), &bytes_written);

        TEST_ASSERT_EQUAL(SERIALIZER_OK, err);
        TEST_ASSERT_EQUAL(48, bytes_written);

        struct decode_data data = {0};

        err = s_tlv_decode(buffer, bytes_written, on_tlv_decode_element, &data);

        TEST_ASSERT_EQUAL(SERIALIZER_OK, err);
        TEST_ASSERT_EQUAL(2, data.tlv_el_num);

        { // check each decoded element
            TEST_ASSERT_EQUAL_INT(0, data.tlv_els[0].idx);
            TEST_ASSERT_EQUAL_INT(0, data.tlv_els[0].level);
            TEST_ASSERT_EQUAL_INT(ENUM_VALUE_3, *(int*) data.tlv_els[0].value);

            TEST_ASSERT_EQUAL_INT(1, data.tlv_els[1].idx);
            TEST_ASSERT_EQUAL_INT(0, data.tlv_els[1].level);
            TEST_ASSERT_EQUAL_STRING("Hello, World!",
                                     (const char*) data.tlv_els[1].value);
        }
    }
}

void test_tlv_encode_buffer_too_small() {
    simple_struct ss = {
        .id = 42,
        .value = 3.14f,
        .active = true,
        .name = "Hello, World!",
        .passport_number = "1234567890",
        .blob = {0x01, 0x02, 0x03, 0x04},
    };

    uint8_t buffer[64];
    size_t bytes_written = 0;

    const s_type_info* info = S_GET_STRUCT_TYPE_INFO(simple_struct);
    s_serializer_error err = s_tlv_encode(info, &ss, buffer, 1, &bytes_written);

    TEST_ASSERT_EQUAL(SERIALIZER_ERROR_BUFFER_TOO_SMALL, err);
}

// --- partial struct encoding
typedef simple_struct partial_simple_struct;
S_SERIALIZE_BEGIN(partial_simple_struct)
S_FIELD_INT32(id, "Id")
S_FIELD_STRING_CONST(name)
S_SERIALIZE_END()

void test_tlv_encode_decode_partial_struct() {
    partial_simple_struct ss = {
        .id = 42,
        .name = "Hello, World!",
    };

    uint8_t buffer[1024];
    size_t bytes_written = 0;

    const s_type_info* info = S_GET_STRUCT_TYPE_INFO(partial_simple_struct);
    s_serializer_error err =
        s_tlv_encode(info, &ss, buffer, sizeof(buffer), &bytes_written);

    TEST_ASSERT_EQUAL(SERIALIZER_OK, err);
    TEST_ASSERT_EQUAL(30, bytes_written);

    struct decode_data data = {0};

    err = s_tlv_decode(buffer, bytes_written, on_tlv_decode_element, &data);

    TEST_ASSERT_EQUAL(SERIALIZER_OK, err);
    TEST_ASSERT_EQUAL(2, data.tlv_el_num);

    { // check each decoded element
        TEST_ASSERT_EQUAL_INT(0, data.tlv_els[0].idx);
        TEST_ASSERT_EQUAL_INT(0, data.tlv_els[0].level);
        TEST_ASSERT_EQUAL_INT(42, *(int*) data.tlv_els[0].value);

        TEST_ASSERT_EQUAL_INT(1, data.tlv_els[1].idx);
        TEST_ASSERT_EQUAL_INT(0, data.tlv_els[1].level);
        TEST_ASSERT_EQUAL_STRING("Hello, World!",
                                 (const char*) data.tlv_els[1].value);
    }
}

void setUp() {}
void tearDown() {}

int main() {
    UNITY_BEGIN();

    RUN_TEST(test_tlv_encode_decode_simple_struct);
    RUN_TEST(test_tlv_encode_decode_nested_struct);
    RUN_TEST(test_tlv_encode_decode_nested_union_struct);
    RUN_TEST(test_tlv_encode_buffer_too_small);
    RUN_TEST(test_tlv_encode_decode_partial_struct);

    UNITY_END();
    return 0;
}