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

// system includes
#include <stdlib.h>

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

struct decode_data {
    int tlv_el_num;
    s_tlv_decoded_element_data tlv_els[1024];
};

#define MAX_BYTE_DUMP (32)

const char* print_value(uint8_t* data, int len) {
    static char buffer[1024];
    memset(buffer, 0, sizeof(buffer));

    // checks if value is a string -- by checking each byte is within printable
    // range and data is null-terminated
    bool is_string = false;

    if (len > 1)
        for (int i = 0; i < len; i++) {
            if (data[i] == 0 && i == len - 1) {
                is_string = true;
                break;
            }

            if (data[i] < 32 || data[i] > 126) {
                is_string = false;
                break;
            }
        }

    if (is_string) {
        snprintf(buffer, sizeof(buffer), "\"%*s%s\"",
                 (len > MAX_BYTE_DUMP ? MAX_BYTE_DUMP : len - 1), data,
                 len > MAX_BYTE_DUMP ? "..." : "");
    } else {
        buffer[0] = 0;
        for (int i = 0; i < MIN(MAX_BYTE_DUMP / 3, len); i++) {
            snprintf(buffer + strlen(buffer), sizeof(buffer) - strlen(buffer),
                     "%02X ", data[i]);
        }

        if (len > MAX_BYTE_DUMP / 3) {
            snprintf(buffer + strlen(buffer), sizeof(buffer) - strlen(buffer),
                     "...");
        }
    }

    return buffer;
}

void print_decode_data_debug(struct decode_data* data) {
    int total_length = 0;
    for (int i = 0; i < data->tlv_el_num; i++) {
        s_tlv_decoded_element_data* el = &data->tlv_els[i];

        static const char* type_labels[] = {
            "UNKNOWN",          "FIELD",
            "NESTED",           "LIST",
            "NESTED_LIST",      "COMPRESSED_VALUE",
            "ENCRYPTED_VALUE",  "COMPRESSED_NESTED",
            "ENCRYPTED_NESTED",
        };
        const char* type_label = "UNKNOWN";

        if (el->type < sizeof(type_labels) / sizeof(type_labels[0])) {
            type_label = type_labels[el->type];
        }

        printf("%3d LVL %2d %*s%2d %s (%02X) %4db %32s\n", i, el->level,
               el->level * 4, "", el->idx, type_label, el->type, el->length,
               print_value(el->value, el->length));

        total_length += el->length;
    }
    printf("TOTAL VALUE LENGTH: %d\n", total_length);
}

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

    print_decode_data_debug(&data);

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

void test_tlv_encode_decode_struct_with_builtin_arrays() {
    builtin_arrays_struct bas = {
        .n_static_ints = 5,
        .static_ints = {1, 2, 3, 4, 5},
        .n_dynamic_ints = 3,
        .dynamic_ints = (int32_t*) malloc(3 * sizeof(int32_t)),
    };
    for (int i = 0; i < bas.n_dynamic_ints; i++) {
        bas.dynamic_ints[i] = i + 1;
    }

    uint8_t buffer[1024];
    size_t bytes_written = 0;

    const s_type_info* info = S_GET_STRUCT_TYPE_INFO(builtin_arrays_struct);
    s_serializer_error err =
        s_tlv_encode(info, &bas, buffer, sizeof(buffer), &bytes_written);

    TEST_ASSERT_EQUAL(SERIALIZER_OK, err);
    TEST_ASSERT_EQUAL(64, bytes_written);

    struct decode_data data = {0};

    err = s_tlv_decode(buffer, bytes_written, on_tlv_decode_element, &data);
    print_decode_data_debug(&data);

    TEST_ASSERT_EQUAL(SERIALIZER_OK, err);
    TEST_ASSERT_EQUAL(4, data.tlv_el_num);

    { // check each decoded element
        TEST_ASSERT_EQUAL_INT(0, data.tlv_els[0].idx);
        TEST_ASSERT_EQUAL_INT(0, data.tlv_els[0].level);
        TEST_ASSERT_EQUAL_INT(5, *(int*) data.tlv_els[0].value);

        TEST_ASSERT_EQUAL_INT(1, data.tlv_els[1].idx);
        TEST_ASSERT_EQUAL_INT(0, data.tlv_els[1].level);
        TEST_ASSERT_EQUAL_MEMORY(bas.static_ints, data.tlv_els[1].value,
                                 bas.n_static_ints);

        TEST_ASSERT_EQUAL_INT(2, data.tlv_els[2].idx);
        TEST_ASSERT_EQUAL_INT(0, data.tlv_els[2].level);
        TEST_ASSERT_EQUAL_INT(3, *(int*) data.tlv_els[2].value);

        TEST_ASSERT_EQUAL_INT(3, data.tlv_els[3].idx);
        TEST_ASSERT_EQUAL_INT(0, data.tlv_els[3].level);
        TEST_ASSERT_EQUAL_MEMORY(bas.dynamic_ints, data.tlv_els[3].value,
                                 bas.n_dynamic_ints * sizeof(int32_t));
    }
}

void test_tlv_encode_decode_struct_with_struct_arrays() {
    struct_arrays_struct sas = {
        .n_static_structs = 2,
        .static_structs =
            {
                {.id = 1, .name = "12345"},
                {.id = 2, .name = "1234567890"},
            },
        .n_dynamic_structs = 3,
        .dynamic_structs = (simple_struct*) malloc(3 * sizeof(simple_struct)),
    };
    memset(sas.dynamic_structs, 0, sizeof(simple_struct) * 3);

    for (int i = 0; i < sas.n_dynamic_structs; i++) {
        sas.dynamic_structs[i].id = i + 1;
        sas.dynamic_structs[i].name = "Name";
    }

    uint8_t buffer[1024];
    size_t bytes_written = 0;

    const s_type_info* info = S_GET_STRUCT_TYPE_INFO(struct_arrays_struct);
    s_serializer_error err =
        s_tlv_encode(info, &sas, buffer, sizeof(buffer), &bytes_written);

    TEST_ASSERT_EQUAL(SERIALIZER_OK, err);
    TEST_ASSERT_EQUAL(449, bytes_written);

    struct decode_data data = {0};

    err = s_tlv_decode(buffer, bytes_written, on_tlv_decode_element, &data);

    TEST_ASSERT_EQUAL(SERIALIZER_OK, err);

    print_decode_data_debug(&data);
    TEST_ASSERT_EQUAL(32, data.tlv_el_num);

    { // check each decoded element

        // n_static_structs
        TEST_ASSERT_EQUAL_INT(0, data.tlv_els[0].level);
        TEST_ASSERT_EQUAL_INT(0, data.tlv_els[0].idx);
        TEST_ASSERT_EQUAL_INT(TLV_TAG_FIELD, data.tlv_els[0].type);
        TEST_ASSERT_EQUAL_INT(2, *(int*) data.tlv_els[0].value);

        // static structs
        int i = 0;
        int elIdx = 1;
        do {
            int idx = i * 6;

            TEST_ASSERT_EQUAL_INT(1, data.tlv_els[elIdx].level);
            TEST_ASSERT_EQUAL_INT(idx, data.tlv_els[elIdx].idx);
            TEST_ASSERT_EQUAL_INT(TLV_TAG_FIELD, data.tlv_els[elIdx].type);
            TEST_ASSERT_EQUAL_INT(sas.static_structs[i].id,
                                  *(int*) data.tlv_els[elIdx++].value);

            TEST_ASSERT_EQUAL_INT(1, data.tlv_els[elIdx].level);
            TEST_ASSERT_EQUAL_INT(idx + 1, data.tlv_els[elIdx].idx);
            TEST_ASSERT_EQUAL_INT(TLV_TAG_FIELD, data.tlv_els[elIdx].type);
            TEST_ASSERT_EQUAL_FLOAT(sas.static_structs[i].value,
                                    *(float*) data.tlv_els[elIdx++].value);

            TEST_ASSERT_EQUAL_INT(1, data.tlv_els[elIdx].level);
            TEST_ASSERT_EQUAL_INT(idx + 2, data.tlv_els[elIdx].idx);
            TEST_ASSERT_EQUAL_INT(TLV_TAG_FIELD, data.tlv_els[elIdx].type);
            TEST_ASSERT_EQUAL_INT(sas.static_structs[i].active,
                                  *(bool*) data.tlv_els[elIdx++].value);

            TEST_ASSERT_EQUAL_INT(1, data.tlv_els[elIdx].level);
            TEST_ASSERT_EQUAL_INT(idx + 3, data.tlv_els[elIdx].idx);
            TEST_ASSERT_EQUAL_INT(TLV_TAG_FIELD, data.tlv_els[elIdx].type);
            if (sas.static_structs[i].name)
                TEST_ASSERT_EQUAL_STRING(
                    sas.static_structs[i].name,
                    (const char*) data.tlv_els[elIdx++].value);
            else
                TEST_ASSERT_EQUAL_INT(0, data.tlv_els[elIdx++].length);

            TEST_ASSERT_EQUAL_INT(1, data.tlv_els[elIdx].level);
            TEST_ASSERT_EQUAL_INT(idx + 4, data.tlv_els[elIdx].idx);
            TEST_ASSERT_EQUAL_INT(TLV_TAG_FIELD, data.tlv_els[elIdx].type);

            if (sas.static_structs[i].passport_number)
                TEST_ASSERT_EQUAL_STRING(
                    sas.static_structs[i].passport_number,
                    (const char*) data.tlv_els[elIdx++].value);
            else
                TEST_ASSERT_EQUAL_INT(0, data.tlv_els[elIdx++].length);

            TEST_ASSERT_EQUAL_INT(1, data.tlv_els[elIdx].level);
            TEST_ASSERT_EQUAL_INT(idx + 5, data.tlv_els[elIdx].idx);
            TEST_ASSERT_EQUAL_INT(TLV_TAG_FIELD, data.tlv_els[elIdx].type);
            TEST_ASSERT_EQUAL_MEMORY(sas.static_structs[i].blob,
                                     data.tlv_els[elIdx++].value,
                                     sizeof(sas.static_structs[i].blob));

            i += 1;
        } while (i < sas.n_static_structs);

        // n_dynamic_structs
        TEST_ASSERT_EQUAL_INT(0, data.tlv_els[elIdx].level);
        TEST_ASSERT_EQUAL_INT(2, data.tlv_els[elIdx].idx);

        // dynamic structs
        i = 0;
        elIdx += 1;

        do {
            int idx = i * 6;

            TEST_ASSERT_EQUAL_INT(1, data.tlv_els[elIdx].level);
            TEST_ASSERT_EQUAL_INT(idx, data.tlv_els[elIdx].idx);
            TEST_ASSERT_EQUAL_INT(TLV_TAG_FIELD, data.tlv_els[elIdx].type);
            TEST_ASSERT_EQUAL_INT(sas.dynamic_structs[i].id,
                                  *(int*) data.tlv_els[elIdx++].value);

            TEST_ASSERT_EQUAL_INT(1, data.tlv_els[elIdx].level);
            TEST_ASSERT_EQUAL_INT(idx + 1, data.tlv_els[elIdx].idx);
            TEST_ASSERT_EQUAL_INT(TLV_TAG_FIELD, data.tlv_els[elIdx].type);
            TEST_ASSERT_EQUAL_FLOAT(sas.dynamic_structs[i].value,
                                    *(float*) data.tlv_els[elIdx++].value);

            TEST_ASSERT_EQUAL_INT(1, data.tlv_els[elIdx].level);
            TEST_ASSERT_EQUAL_INT(idx + 2, data.tlv_els[elIdx].idx);
            TEST_ASSERT_EQUAL_INT(TLV_TAG_FIELD, data.tlv_els[elIdx].type);
            TEST_ASSERT_EQUAL_INT(sas.dynamic_structs[i].active,
                                  *(bool*) data.tlv_els[elIdx++].value);

            TEST_ASSERT_EQUAL_INT(1, data.tlv_els[elIdx].level);
            TEST_ASSERT_EQUAL_INT(idx + 3, data.tlv_els[elIdx].idx);
            TEST_ASSERT_EQUAL_INT(TLV_TAG_FIELD, data.tlv_els[elIdx].type);
            if (sas.dynamic_structs[i].name)
                TEST_ASSERT_EQUAL_STRING(
                    sas.dynamic_structs[i].name,
                    (const char*) data.tlv_els[elIdx++].value);
            else
                TEST_ASSERT_EQUAL_INT(0, data.tlv_els[elIdx++].length);

            TEST_ASSERT_EQUAL_INT(1, data.tlv_els[elIdx].level);
            TEST_ASSERT_EQUAL_INT(idx + 4, data.tlv_els[elIdx].idx);
            TEST_ASSERT_EQUAL_INT(TLV_TAG_FIELD, data.tlv_els[elIdx].type);

            if (sas.dynamic_structs[i].passport_number)
                TEST_ASSERT_EQUAL_STRING(
                    sas.dynamic_structs[i].passport_number,
                    (const char*) data.tlv_els[elIdx++].value);
            else

                TEST_ASSERT_EQUAL_INT(0, data.tlv_els[elIdx++].length);

            TEST_ASSERT_EQUAL_INT(1, data.tlv_els[elIdx].level);
            TEST_ASSERT_EQUAL_INT(idx + 5, data.tlv_els[elIdx].idx);
            TEST_ASSERT_EQUAL_INT(TLV_TAG_FIELD, data.tlv_els[elIdx].type);
            TEST_ASSERT_EQUAL_MEMORY(sas.dynamic_structs[i].blob,
                                     data.tlv_els[elIdx++].value,
                                     sizeof(sas.dynamic_structs[i].blob));

            i += 1;
        } while (i < sas.n_dynamic_structs);
    }
}

void test_tlv_encode_decode_struct_with_fixed_strings() {
    fixed_strings_struct fss = {
        .name = "Hello, World!",
        .n_phone_numbers = 2,
        .phone_numbers = {"1234567890", "0987654321"},
    };

    uint8_t buffer[1024];
    size_t bytes_written = 0;

    const s_type_info* info = S_GET_STRUCT_TYPE_INFO(fixed_strings_struct);
    s_serializer_error err =
        s_tlv_encode(info, &fss, buffer, sizeof(buffer), &bytes_written);

    TEST_ASSERT_EQUAL(SERIALIZER_OK, err);
    TEST_ASSERT_EQUAL(58, bytes_written);

    struct decode_data data = {0};

    err = s_tlv_decode(buffer, bytes_written, on_tlv_decode_element, &data);

    TEST_ASSERT_EQUAL(SERIALIZER_OK, err);

    print_decode_data_debug(&data);

    TEST_ASSERT_EQUAL(3, data.tlv_el_num);

    { // check each decoded element
        TEST_ASSERT_EQUAL_INT(0, data.tlv_els[0].idx);
        TEST_ASSERT_EQUAL_INT(0, data.tlv_els[0].level);
        TEST_ASSERT_EQUAL_STRING("Hello, World!",
                                 (const char*) data.tlv_els[0].value);

        TEST_ASSERT_EQUAL_INT(1, data.tlv_els[1].idx);
        TEST_ASSERT_EQUAL_INT(0, data.tlv_els[1].level);
        TEST_ASSERT_EQUAL_INT(2, *(int*) data.tlv_els[1].value);

        TEST_ASSERT_EQUAL_INT(2, data.tlv_els[2].idx);
        TEST_ASSERT_EQUAL_INT(0, data.tlv_els[2].level);
        TEST_ASSERT_EQUAL_STRING("1234567890",
                                 (const char*) data.tlv_els[2].value);
    }
}

void test_tlv_encode_decode_test_structs() {
    {
        TestStruct3 ts3 = {
            .nStaticStructs_ = 3,
            .nDynamicStructs_ = 3,
            .nInts_ = 3,
            .nInts2_ = 2,
            .staticStructs_ =
                {
                    {.str_ = "Hello, World0!", .a_ = 1},
                    {.str_ = "Hello, World1!", .a_ = 2},
                    {.str_ = "Hello, World2!", .a_ = 3},
                },
            .dynamicStructs_ = (TestStruct*) malloc(3 * sizeof(TestStruct)),
            .ints_ = {1, 2, 3},
            .ints2_ = (int*) malloc(2 * sizeof(int)),
        };

        for (int i = 0; i < ts3.nDynamicStructs_; i++) {
            snprintf(ts3.dynamicStructs_[i].testStruct_.str_, 32,
                     "Hello, World%d!", i + 3);
            ts3.dynamicStructs_[i].testStruct_.a_ = i + 4;
        }

        for (int i = 0; i < ts3.nInts2_; i++) {
            ts3.ints2_[i] = i + 1;
        }

        uint8_t buffer[1024];
        size_t bytes_written = 0;

        const s_type_info* info = S_GET_STRUCT_TYPE_INFO(TestStruct3);
        s_serializer_error err =
            s_tlv_encode(info, &ts3, buffer, sizeof(buffer), &bytes_written);

        TEST_ASSERT_EQUAL(SERIALIZER_OK, err);

        struct decode_data data = {0};

        err = s_tlv_decode(buffer, bytes_written, on_tlv_decode_element, &data);

        TEST_ASSERT_EQUAL(SERIALIZER_OK, err);

        print_decode_data_debug(&data);
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
    RUN_TEST(test_tlv_encode_decode_struct_with_builtin_arrays);
    RUN_TEST(test_tlv_encode_decode_struct_with_struct_arrays);
    RUN_TEST(test_tlv_encode_decode_struct_with_fixed_strings);

    RUN_TEST(test_tlv_encode_decode_test_structs);

    UNITY_END();
    return 0;
}