/*
 * Created on Thu Jan 09 2025
 *
 * Author: Peter Gusev
 * Copyright (c) 2025 Peter Gusev. All rights reserved.
 */

#include "common.h"

// unity
#include <unity.h>

// system includes
#include <stdlib.h>

static s_allocator g_default_allocator = {
    .allocate = malloc,
    .deallocate = free,
};

void test_serialize_deserialize_simple_struct() {
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
    s_serialize_options opts = {0};
    s_serializer_error err =
        s_serialize(opts, S_GET_STRUCT_TYPE_INFO(simple_struct), &ss, buffer,
                    sizeof(buffer), &bytes_written);

    TEST_ASSERT_EQUAL(SERIALIZER_OK, err);
    TEST_ASSERT_EQUAL(102, bytes_written);

    simple_struct deserialized_ss = {0};

    s_deserialize_options dopts = {
        .format = FORMAT_C_STRUCT,
        .allocator = &g_default_allocator,
    };

    err = s_deserialize(dopts, S_GET_STRUCT_TYPE_INFO(simple_struct),
                        &deserialized_ss, buffer, bytes_written);

    TEST_ASSERT_EQUAL(SERIALIZER_OK, err);
    TEST_ASSERT_EQUAL_INT(42, deserialized_ss.id);
    TEST_ASSERT_EQUAL_FLOAT(3.14f, deserialized_ss.value);
    TEST_ASSERT_EQUAL_INT(1, deserialized_ss.active);
    TEST_ASSERT_EQUAL_STRING("Hello, World!", deserialized_ss.name);
    TEST_ASSERT_EQUAL_STRING("1234567890", deserialized_ss.passport_number);
    TEST_ASSERT_EQUAL_INT(0x01, deserialized_ss.blob[0]);
    TEST_ASSERT_EQUAL_INT(0x02, deserialized_ss.blob[1]);
    TEST_ASSERT_EQUAL_INT(0x03, deserialized_ss.blob[2]);
    TEST_ASSERT_EQUAL_INT(0x04, deserialized_ss.blob[3]);
}

void test_serialize_deserialize_with_empty_and_null_string() {
    simple_struct ss = {
        .id = 42,
        .value = 3.14f,
        .active = true,
        .name = "",
        .passport_number = NULL,
        .blob = {0x01, 0x02, 0x03, 0x04},
    };

    uint8_t buffer[1024];
    size_t bytes_written = 0;

    const s_type_info* info = S_GET_STRUCT_TYPE_INFO(simple_struct);
    s_serialize_options opts = {0};
    s_serializer_error err =
        s_serialize(opts, S_GET_STRUCT_TYPE_INFO(simple_struct), &ss, buffer,
                    sizeof(buffer), &bytes_written);

    TEST_ASSERT_EQUAL(SERIALIZER_OK, err);
    TEST_ASSERT_EQUAL(78, bytes_written);

    simple_struct deserialized_ss = {0};

    s_deserialize_options dopts = {
        .format = FORMAT_C_STRUCT,
        .allocator = &g_default_allocator,
    };

    err = s_deserialize(dopts, S_GET_STRUCT_TYPE_INFO(simple_struct),
                        &deserialized_ss, buffer, bytes_written);

    TEST_ASSERT_EQUAL(SERIALIZER_OK, err);
    TEST_ASSERT_EQUAL_INT(42, deserialized_ss.id);
    TEST_ASSERT_EQUAL_FLOAT(3.14f, deserialized_ss.value);
    TEST_ASSERT_EQUAL_INT(1, deserialized_ss.active);
    TEST_ASSERT_EQUAL_STRING("", deserialized_ss.name);
    TEST_ASSERT_NULL(deserialized_ss.passport_number);
    TEST_ASSERT_EQUAL_INT(0x01, deserialized_ss.blob[0]);
    TEST_ASSERT_EQUAL_INT(0x02, deserialized_ss.blob[1]);
    TEST_ASSERT_EQUAL_INT(0x03, deserialized_ss.blob[2]);
    TEST_ASSERT_EQUAL_INT(0x04, deserialized_ss.blob[3]);
}

void test_serialize_deserialize_nested_struct() {
    nested_struct ns = {
        .id = ENUM_VALUE_1,
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
    s_serialize_options opts = {0};
    s_serializer_error err =
        s_serialize(opts, S_GET_STRUCT_TYPE_INFO(nested_struct), &ns, buffer,
                    sizeof(buffer), &bytes_written);

    TEST_ASSERT_EQUAL(SERIALIZER_OK, err);
    TEST_ASSERT_EQUAL(139, bytes_written);

    nested_struct deserialized_ns = {0};

    s_deserialize_options dopts = {
        .format = FORMAT_C_STRUCT,
        .allocator = &g_default_allocator,
    };

    err = s_deserialize(dopts, S_GET_STRUCT_TYPE_INFO(nested_struct),
                        &deserialized_ns, buffer, bytes_written);

    TEST_ASSERT_EQUAL(SERIALIZER_OK, err);
    TEST_ASSERT_EQUAL_INT(ENUM_VALUE_1, deserialized_ns.id);
    TEST_ASSERT_EQUAL_INT(42, deserialized_ns.sub.id);
    TEST_ASSERT_EQUAL_FLOAT(3.14f, deserialized_ns.sub.value);
    TEST_ASSERT_EQUAL_INT(1, deserialized_ns.sub.active);
    TEST_ASSERT_EQUAL_STRING("Hello, World!", deserialized_ns.sub.name);
    TEST_ASSERT_EQUAL_STRING("1234567890", deserialized_ns.sub.passport_number);
    TEST_ASSERT_EQUAL_INT(0x01, deserialized_ns.sub.blob[0]);
    TEST_ASSERT_EQUAL_INT(0x02, deserialized_ns.sub.blob[1]);
    TEST_ASSERT_EQUAL_INT(0x03, deserialized_ns.sub.blob[2]);
    TEST_ASSERT_EQUAL_INT(0x04, deserialized_ns.sub.blob[3]);
    TEST_ASSERT_EQUAL_STRING("Hello, World2!", deserialized_ns.name);
}

// test super nested structs
typedef struct {
    nested_struct sub;
    int id;
    simple_struct ss;
} super_nested_struct;

S_SERIALIZE_BEGIN(super_nested_struct)
S_FIELD_STRUCT(sub, nested_struct)
S_FIELD_INT32(id)
S_FIELD_STRUCT(ss, simple_struct)
S_SERIALIZE_END()

void test_serialize_deserialize_super_nested_struct() {
    super_nested_struct sns = {
        .sub =
            {
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
            },
        .id = 123,
        .ss =
            {
                .id = 42,
                .value = 3.14f,
                .active = true,
                .name = "Hello, World!",
                .passport_number = "1234567890",
                .blob = {0x01, 0x02, 0x03, 0x04},
            },
    };

    uint8_t buffer[1024];
    size_t bytes_written = 0;

    const s_type_info* info = S_GET_STRUCT_TYPE_INFO(super_nested_struct);
    s_serialize_options opts = {0};
    s_serializer_error err =
        s_serialize(opts, S_GET_STRUCT_TYPE_INFO(super_nested_struct), &sns,
                    buffer, sizeof(buffer), &bytes_written);

    TEST_ASSERT_EQUAL(SERIALIZER_OK, err);
    TEST_ASSERT_EQUAL(263, bytes_written);

    super_nested_struct deserialized_sns = {0};

    s_deserialize_options dopts = {
        .format = FORMAT_C_STRUCT,
        .allocator = &g_default_allocator,
    };

    err = s_deserialize(dopts, S_GET_STRUCT_TYPE_INFO(super_nested_struct),
                        &deserialized_sns, buffer, bytes_written);

    TEST_ASSERT_EQUAL(SERIALIZER_OK, err);
    TEST_ASSERT_EQUAL_INT(ENUM_VALUE_2, deserialized_sns.sub.id);
    TEST_ASSERT_EQUAL_INT(42, deserialized_sns.sub.sub.id);
    TEST_ASSERT_EQUAL_FLOAT(3.14f, deserialized_sns.sub.sub.value);
    TEST_ASSERT_EQUAL_INT(1, deserialized_sns.sub.sub.active);
    TEST_ASSERT_EQUAL_STRING("Hello, World!", deserialized_sns.sub.sub.name);
    TEST_ASSERT_EQUAL_STRING("1234567890",
                             deserialized_sns.sub.sub.passport_number);
    TEST_ASSERT_EQUAL_INT(0x01, deserialized_sns.sub.sub.blob[0]);
    TEST_ASSERT_EQUAL_INT(0x02, deserialized_sns.sub.sub.blob[1]);
    TEST_ASSERT_EQUAL_INT(0x03, deserialized_sns.sub.sub.blob[2]);
    TEST_ASSERT_EQUAL_INT(0x04, deserialized_sns.sub.sub.blob[3]);
    TEST_ASSERT_EQUAL_STRING("Hello, World2!", deserialized_sns.sub.name);
    TEST_ASSERT_EQUAL_INT(123, deserialized_sns.id);
    TEST_ASSERT_EQUAL_INT(42, deserialized_sns.ss.id);
    TEST_ASSERT_EQUAL_FLOAT(3.14f, deserialized_sns.ss.value);
    TEST_ASSERT_EQUAL_INT(1, deserialized_sns.ss.active);
    TEST_ASSERT_EQUAL_STRING("Hello, World!", deserialized_sns.ss.name);
    TEST_ASSERT_EQUAL_STRING("1234567890", deserialized_sns.ss.passport_number);
    TEST_ASSERT_EQUAL_INT(0x01, deserialized_sns.ss.blob[0]);
    TEST_ASSERT_EQUAL_INT(0x02, deserialized_sns.ss.blob[1]);
    TEST_ASSERT_EQUAL_INT(0x03, deserialized_sns.ss.blob[2]);
    TEST_ASSERT_EQUAL_INT(0x04, deserialized_sns.ss.blob[3]);
}

void test_serialize_deserialize_union_structs() {
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
        s_serialize_options opts = {0};
        s_serializer_error err =
            s_serialize(opts, S_GET_STRUCT_TYPE_INFO(nested_union_struct), &ns,
                        buffer, sizeof(buffer), &bytes_written);

        TEST_ASSERT_EQUAL(SERIALIZER_OK, err);
        TEST_ASSERT_EQUAL(118, bytes_written);

        nested_union_struct deserialized_ns = {0};

        s_deserialize_options dopts = {
            .format = FORMAT_C_STRUCT,
            .allocator = &g_default_allocator,
        };

        err = s_deserialize(dopts, S_GET_STRUCT_TYPE_INFO(nested_union_struct),
                            &deserialized_ns, buffer, bytes_written);

        TEST_ASSERT_EQUAL(SERIALIZER_OK, err);
        TEST_ASSERT_EQUAL_INT(ENUM_VALUE_1, deserialized_ns.id);
        TEST_ASSERT_EQUAL_INT(42, deserialized_ns.data.sub.id);
        TEST_ASSERT_EQUAL_FLOAT(3.14f, deserialized_ns.data.sub.value);
        TEST_ASSERT_EQUAL_INT(1, deserialized_ns.data.sub.active);
        TEST_ASSERT_EQUAL_STRING("Hello, World!",
                                 deserialized_ns.data.sub.name);
        TEST_ASSERT_EQUAL_STRING("1234567890",
                                 deserialized_ns.data.sub.passport_number);
        TEST_ASSERT_EQUAL_INT(0x01, deserialized_ns.data.sub.blob[0]);
        TEST_ASSERT_EQUAL_INT(0x02, deserialized_ns.data.sub.blob[1]);
        TEST_ASSERT_EQUAL_INT(0x03, deserialized_ns.data.sub.blob[2]);
        TEST_ASSERT_EQUAL_INT(0x04, deserialized_ns.data.sub.blob[3]);
    }
    {
        nested_union_struct ns = {
            .id = ENUM_VALUE_2,
            .data =
                {
                    .str = "Hello, World!",
                },
        };

        uint8_t buffer[1024];
        size_t bytes_written = 0;

        const s_type_info* info = S_GET_STRUCT_TYPE_INFO(nested_union_struct);
        s_serialize_options opts = {0};
        s_serializer_error err =
            s_serialize(opts, S_GET_STRUCT_TYPE_INFO(nested_union_struct), &ns,
                        buffer, sizeof(buffer), &bytes_written);

        TEST_ASSERT_EQUAL(SERIALIZER_OK, err);
        TEST_ASSERT_EQUAL(36, bytes_written);

        nested_union_struct deserialized_ns = {0};

        s_deserialize_options dopts = {
            .format = FORMAT_C_STRUCT,
            .allocator = &g_default_allocator,
        };

        err = s_deserialize(dopts, S_GET_STRUCT_TYPE_INFO(nested_union_struct),
                            &deserialized_ns, buffer, bytes_written);

        TEST_ASSERT_EQUAL(SERIALIZER_OK, err);
        TEST_ASSERT_EQUAL_INT(ENUM_VALUE_2, deserialized_ns.id);
        TEST_ASSERT_EQUAL_STRING("Hello, World!", deserialized_ns.data.str.str);
    }
    {
        nested_union_struct ns = {
            .id = ENUM_VALUE_3,
            .data =
                {
                    .value = 42,
                },
        };

        uint8_t buffer[1024];
        size_t bytes_written = 0;

        const s_type_info* info = S_GET_STRUCT_TYPE_INFO(nested_union_struct);
        s_serialize_options opts = {0};
        s_serializer_error err =
            s_serialize(opts, S_GET_STRUCT_TYPE_INFO(nested_union_struct), &ns,
                        buffer, sizeof(buffer), &bytes_written);

        TEST_ASSERT_EQUAL(SERIALIZER_OK, err);
        TEST_ASSERT_EQUAL(20, bytes_written);

        nested_union_struct deserialized_ns = {0};

        s_deserialize_options dopts = {
            .format = FORMAT_C_STRUCT,
            .allocator = &g_default_allocator,
        };

        err = s_deserialize(dopts, S_GET_STRUCT_TYPE_INFO(nested_union_struct),
                            &deserialized_ns, buffer, bytes_written);

        TEST_ASSERT_EQUAL(SERIALIZER_OK, err);
        TEST_ASSERT_EQUAL_INT(ENUM_VALUE_3, deserialized_ns.id);
        TEST_ASSERT_EQUAL_INT(42, deserialized_ns.data.value);
    }
    {
        nested_union_struct ns = {
            .id = ENUM_VALUE_4,
            .data =
                {
                    .name = "Hello, World!",
                },
        };

        uint8_t buffer[1024];
        size_t bytes_written = 0;

        const s_type_info* info = S_GET_STRUCT_TYPE_INFO(nested_union_struct);
        s_serialize_options opts = {0};
        s_serializer_error err =
            s_serialize(opts, S_GET_STRUCT_TYPE_INFO(nested_union_struct), &ns,
                        buffer, sizeof(buffer), &bytes_written);

        TEST_ASSERT_EQUAL(SERIALIZER_OK, err);
        TEST_ASSERT_EQUAL(48, bytes_written);

        nested_union_struct deserialized_ns = {0};

        s_deserialize_options dopts = {
            .format = FORMAT_C_STRUCT,
            .allocator = &g_default_allocator,
        };

        err = s_deserialize(dopts, S_GET_STRUCT_TYPE_INFO(nested_union_struct),
                            &deserialized_ns, buffer, bytes_written);

        TEST_ASSERT_EQUAL(SERIALIZER_OK, err);
        TEST_ASSERT_EQUAL_INT(ENUM_VALUE_4, deserialized_ns.id);
        TEST_ASSERT_EQUAL_STRING("Hello, World!", deserialized_ns.data.name);
    }
}

void test_serialize_deserialize_into_json_string() {
    {
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
        s_serialize_options opts = {0};
        s_serializer_error err =
            s_serialize(opts, S_GET_STRUCT_TYPE_INFO(simple_struct), &ss,
                        buffer, sizeof(buffer), &bytes_written);

        TEST_ASSERT_EQUAL(SERIALIZER_OK, err);
        TEST_ASSERT_EQUAL(102, bytes_written);

        char deserialized_json[1024] = {0};

        s_deserialize_options dopts = {
            .format = FORMAT_JSON_STRING,
            .allocator = &g_default_allocator,
        };

        err = s_deserialize(dopts, S_GET_STRUCT_TYPE_INFO(simple_struct),
                            deserialized_json, buffer, bytes_written);

        TEST_ASSERT_EQUAL(SERIALIZER_OK, err);
        TEST_ASSERT_EQUAL_STRING(
            "{\"Id\":42,\"value\":3.140000,\"active\":true,\"name\":\"Hello, "
            "World!\",\"PassportNumber\":\"1234567890\",\"Data\":[1,2,3,4,0,0,"
            "0,0,"
            "0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0]}",
            deserialized_json);
    }
    {
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
        s_serialize_options opts = {0};
        s_serializer_error err =
            s_serialize(opts, S_GET_STRUCT_TYPE_INFO(nested_struct), &ns,
                        buffer, sizeof(buffer), &bytes_written);

        TEST_ASSERT_EQUAL(SERIALIZER_OK, err);
        TEST_ASSERT_EQUAL(139, bytes_written);

        char deserialized_json[1024] = {0};

        s_deserialize_options dopts = {
            .format = FORMAT_JSON_STRING,
            .allocator = &g_default_allocator,
        };

        err = s_deserialize(dopts, S_GET_STRUCT_TYPE_INFO(nested_struct),
                            deserialized_json, buffer, bytes_written);

        TEST_ASSERT_EQUAL(SERIALIZER_OK, err);
        TEST_ASSERT_EQUAL_STRING(
            "{\"id\":1,\"sub\":{\"Id\":42,\"value\":3.140000,\"active\":true,"
            "\"name\":\"Hello, World!\",\"PassportNumber\":\"1234567890\","
            "\"Data\":[1,2,3,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,"
            "0,0,0,0]},\"name\":"
            "\"Hello, World2!\"}",
            deserialized_json);
    }
    {
        super_nested_struct sns = {
            .sub =
                {
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
                },
            .id = 123,
            .ss =
                {
                    .id = 42,
                    .value = 3.14f,
                    .active = true,
                    .name = "Hello, World!",
                    .passport_number = "1234567890",
                    .blob = {0x01, 0x02, 0x03, 0x04},
                },
        };

        uint8_t buffer[1024];
        size_t bytes_written = 0;

        const s_type_info* info = S_GET_STRUCT_TYPE_INFO(super_nested_struct);
        s_serialize_options opts = {0};
        s_serializer_error err =
            s_serialize(opts, S_GET_STRUCT_TYPE_INFO(super_nested_struct), &sns,
                        buffer, sizeof(buffer), &bytes_written);

        TEST_ASSERT_EQUAL(SERIALIZER_OK, err);
        TEST_ASSERT_EQUAL(263, bytes_written);

        char deserialized_json[1024] = {0};

        s_deserialize_options dopts = {
            .format = FORMAT_JSON_STRING,
            .allocator = &g_default_allocator,
        };

        err = s_deserialize(dopts, S_GET_STRUCT_TYPE_INFO(super_nested_struct),
                            deserialized_json, buffer, bytes_written);

        TEST_ASSERT_EQUAL(SERIALIZER_OK, err);
        TEST_ASSERT_EQUAL_STRING(
            "{\"sub\":{\"id\":1,\"sub\":{\"Id\":42,\"value\":3.140000,"
            "\"active\":true,\"name\":\"Hello, World!\",\"PassportNumber\":"
            "\"1234567890\",\"Data\":[1,2,3,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,"
            "0,0,0,0,0,0,0,0,0,0,0,0,0,0]},\"name\":\"Hello, World2!\"},"
            "\"id\":123,\"ss\":{\"Id\":42,\"value\":3.140000,\"active\":true,"
            "\"name\":\"Hello, World!\",\"PassportNumber\":\"1234567890\","
            "\"Data\":[1,2,3,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,"
            "0,0,0,0,0]}}",
            deserialized_json);
    }
}

void test_serialize_deserialize_struct_with_arrays() {
    { // array of builtins
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
        s_serialize_options opts = {0};
        s_serializer_error err =
            s_serialize(opts, S_GET_STRUCT_TYPE_INFO(builtin_arrays_struct),
                        &bas, buffer, sizeof(buffer), &bytes_written);

        TEST_ASSERT_EQUAL(SERIALIZER_OK, err);
        TEST_ASSERT_EQUAL(64, bytes_written);

        builtin_arrays_struct deserialized_bas = {0};

        s_deserialize_options dopts = {
            .format = FORMAT_C_STRUCT,
            .allocator = &g_default_allocator,
        };

        err =
            s_deserialize(dopts, S_GET_STRUCT_TYPE_INFO(builtin_arrays_struct),
                          &deserialized_bas, buffer, bytes_written);

        TEST_ASSERT_EQUAL(SERIALIZER_OK, err);
        TEST_ASSERT_EQUAL_INT(5, deserialized_bas.n_static_ints);
        for (int i = 0; i < deserialized_bas.n_static_ints; i++) {
            TEST_ASSERT_EQUAL_INT(i + 1, deserialized_bas.static_ints[i]);
        }
        TEST_ASSERT_EQUAL_INT(3, deserialized_bas.n_dynamic_ints);
        for (int i = 0; i < deserialized_bas.n_dynamic_ints; i++) {
            TEST_ASSERT_EQUAL_INT(i + 1, deserialized_bas.dynamic_ints[i]);
        }

        g_default_allocator.deallocate(deserialized_bas.dynamic_ints, NULL);
        free(bas.dynamic_ints);
    }

    { // array of structs
        struct_arrays_struct sas = {
            .n_static_structs = 2,
            .static_structs =
                {
                    {.id = 1, .name = "12345"},
                    {.id = 2, .name = "1234567890"},
                },
            .n_dynamic_structs = 3,
            .dynamic_structs =
                (simple_struct*) malloc(3 * sizeof(simple_struct)),
        };
        memset(sas.dynamic_structs, 0, sizeof(simple_struct) * 3);

        for (int i = 0; i < sas.n_dynamic_structs; i++) {
            sas.dynamic_structs[i].id = i + 1;
            sas.dynamic_structs[i].name = "Name";
        }

        uint8_t buffer[1024];
        size_t bytes_written = 0;

        const s_type_info* info = S_GET_STRUCT_TYPE_INFO(struct_arrays_struct);
        s_serialize_options opts = {0};
        s_serializer_error err =
            s_serialize(opts, S_GET_STRUCT_TYPE_INFO(struct_arrays_struct),
                        &sas, buffer, sizeof(buffer), &bytes_written);

        TEST_ASSERT_EQUAL(SERIALIZER_OK, err);
        TEST_ASSERT_EQUAL(449, bytes_written);

        struct_arrays_struct deserialized_sas = {0};

        s_deserialize_options dopts = {
            .format = FORMAT_C_STRUCT,
            .allocator = &g_default_allocator,
        };

        err = s_deserialize(dopts, S_GET_STRUCT_TYPE_INFO(struct_arrays_struct),
                            &deserialized_sas, buffer, bytes_written);

        TEST_ASSERT_EQUAL(SERIALIZER_OK, err);
        TEST_ASSERT_EQUAL_INT(2, deserialized_sas.n_static_structs);
        for (int i = 0; i < deserialized_sas.n_static_structs; i++) {
            TEST_ASSERT_EQUAL_INT(i + 1, deserialized_sas.static_structs[i].id);
            TEST_ASSERT_EQUAL_STRING(i ? "1234567890" : "12345",
                                     deserialized_sas.static_structs[i].name);
        }
        TEST_ASSERT_EQUAL_INT(3, deserialized_sas.n_dynamic_structs);
        for (int i = 0; i < deserialized_sas.n_dynamic_structs; i++) {
            TEST_ASSERT_EQUAL_INT(i + 1,
                                  deserialized_sas.dynamic_structs[i].id);
            TEST_ASSERT_EQUAL_STRING("Name",
                                     deserialized_sas.dynamic_structs[i].name);
        }

        g_default_allocator.deallocate(deserialized_sas.dynamic_structs, NULL);
    }
}

void test_serialize_deserialize_arrays_into_json_string() {
    { // array of builtins
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
        s_serialize_options opts = {0};
        s_serializer_error err =
            s_serialize(opts, S_GET_STRUCT_TYPE_INFO(builtin_arrays_struct),
                        &bas, buffer, sizeof(buffer), &bytes_written);

        TEST_ASSERT_EQUAL(SERIALIZER_OK, err);
        TEST_ASSERT_EQUAL(64, bytes_written);

        char deserialized_json[1024] = {0};

        s_deserialize_options dopts = {
            .format = FORMAT_JSON_STRING,
            .allocator = &g_default_allocator,
        };

        err =
            s_deserialize(dopts, S_GET_STRUCT_TYPE_INFO(builtin_arrays_struct),
                          deserialized_json, buffer, bytes_written);

        TEST_ASSERT_EQUAL(SERIALIZER_OK, err);
        TEST_ASSERT_EQUAL_STRING("{\"n_static_ints\":5,\"StaticInts\":[1,2,3,"
                                 "4,5],\"n_dynamic_ints\":"
                                 "3,\"DynamicInts\":[1,2,3]}",
                                 deserialized_json);

        g_default_allocator.deallocate(bas.dynamic_ints, NULL);
    }

    { // array of structs
        struct_arrays_struct sas = {
            .n_static_structs = 2,
            .static_structs =
                {
                    {.id = 1, .name = "12345"},
                    {.id = 2, .name = "1234567890"},
                },
            .n_dynamic_structs = 3,
            .dynamic_structs =
                (simple_struct*) malloc(3 * sizeof(simple_struct)),
        };
        memset(sas.dynamic_structs, 0, sizeof(simple_struct) * 3);

        for (int i = 0; i < sas.n_dynamic_structs; i++) {
            sas.dynamic_structs[i].id = i + 1;
            sas.dynamic_structs[i].name = "Name";
        }

        uint8_t buffer[1024];
        size_t bytes_written = 0;

        const s_type_info* info = S_GET_STRUCT_TYPE_INFO(struct_arrays_struct);
        s_serialize_options opts = {0};
        s_serializer_error err =
            s_serialize(opts, S_GET_STRUCT_TYPE_INFO(struct_arrays_struct),
                        &sas, buffer, sizeof(buffer), &bytes_written);

        TEST_ASSERT_EQUAL(SERIALIZER_OK, err);
        TEST_ASSERT_EQUAL(449, bytes_written);

        char deserialized_json[1024] = {0};

        s_deserialize_options dopts = {
            .format = FORMAT_JSON_STRING,
            .allocator = &g_default_allocator,
        };

        err = s_deserialize(dopts, S_GET_STRUCT_TYPE_INFO(struct_arrays_struct),
                            deserialized_json, buffer, bytes_written);

        TEST_ASSERT_EQUAL(SERIALIZER_OK, err);
        const char* expected_json =
            "{\"n_static_structs\":2,\"static_structs\":[{\"Id\":1,\"value\":0."
            "000000,"
            "\"active\":false,\"name\":"
            "\"12345\",\"PassportNumber\":\"\",\"Data\":[0,0,0,0,0,0,0,0,0,0,0,"
            "0,0,0,0,0,0,0,0,0,0,0,0,0,"
            "0,0,0,0,0,0,0,0]},{\"Id\":2,\"value\":0.000000,\"active\":false,"
            "\"name\":"
            "\"1234567890\",\"PassportNumber"
            "\":\"\",\"Data\":[0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,"
            "0,0,0,0,0,0,0,0]}],"
            "\"n_dynamic_structs\":3,\"DynamicStructs\":[{\"Id\":1,\"value\":"
            "0.000000,\"active\":false,"
            "\"name\":\"Name\",\"PassportNumber\":\"\",\"Data\":[0,0,0,0,0,0,0,"
            "0,0,0,0,0,0,0,0,0,0,0,0,0,"
            "0,0,0,0,0,0,0,0,0,0,0,0]},{\"Id\":2,\"value\":0.000000,\"active\":"
            "false,"
            "\"name\":\"Name\",\"PassportNumber"
            "\":\"\",\"Data\":[0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,"
            "0,0,0,0,0,0,0,0]},"
            "{\"Id\":3,\"value\":0.000000,\"active\":false,\"name\":\"Name\","
            "\"PassportNumber\":\"\",\"Data\":[0,0,0,0,"
            "0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0]}]}";

        TEST_ASSERT_EQUAL_STRING(expected_json, deserialized_json);
        // TEST_ASSERT_EQUAL_STRING(
        //     "{\"n_static_structs\":2,\"static_structs\":[{\"id\":1,\"name\":"
        //     "\"12345"
        //     "\"},{\"id\":2,\"name\":\"1234567890\"}],\"n_dynamic_structs\":3,"
        //     "\"dynamic_structs\":[{\"Id\":1,\"value\":0.000000,\"active\":"
        //     "false,"
        //     "\"name\":\"Name\",\"PassportNumber\":"
        //     ",\"Data\":[0,0,0,0,0,0,"
        //     "0,0,"
        //     "0,",
        //     deserialized_json);

        g_default_allocator.deallocate(sas.dynamic_structs, NULL);
    }
}

void tests_seialize_deserialize_struct_with_fixed_strings() {
    fixed_strings_struct fss = {
        .name = "Hello, World!",
        .n_phone_numbers = 2,
        .phone_numbers = {"1234567890", "0987654321"},
    };

    uint8_t buffer[1024];
    size_t bytes_written = 0;

    const s_type_info* info = S_GET_STRUCT_TYPE_INFO(fixed_strings_struct);
    s_serialize_options opts = {0};
    s_serializer_error err =
        s_serialize(opts, S_GET_STRUCT_TYPE_INFO(fixed_strings_struct), &fss,
                    buffer, sizeof(buffer), &bytes_written);

    TEST_ASSERT_EQUAL(SERIALIZER_OK, err);
    TEST_ASSERT_EQUAL(58, bytes_written);

    fixed_strings_struct deserialized_fss = {0};

    s_deserialize_options dopts = {
        .format = FORMAT_C_STRUCT,
        .allocator = &g_default_allocator,
    };

    err = s_deserialize(dopts, S_GET_STRUCT_TYPE_INFO(fixed_strings_struct),
                        &deserialized_fss, buffer, bytes_written);

    TEST_ASSERT_EQUAL(SERIALIZER_OK, err);
    TEST_ASSERT_EQUAL_STRING("Hello, World!", deserialized_fss.name);
    TEST_ASSERT_EQUAL_INT(2, deserialized_fss.n_phone_numbers);
    TEST_ASSERT_EQUAL_STRING("1234567890", deserialized_fss.phone_numbers[0]);
    TEST_ASSERT_EQUAL_STRING("0987654321", deserialized_fss.phone_numbers[1]);
}

void test_serialize_deserialize_test_structs() {
    uint8_t buffer[1024];
    size_t bytes_written = 0;

    {
        TestStruct2 ts2 = {.testStruct_ = {.str_ = "Hello, World!", .a_ = 42}};
        const s_type_info* info = S_GET_STRUCT_TYPE_INFO(TestStruct2);
        s_serialize_options opts = {0};
        s_serializer_error err =
            s_serialize(opts, S_GET_STRUCT_TYPE_INFO(TestStruct2), &ts2, buffer,
                        sizeof(buffer), &bytes_written);

        TEST_ASSERT_EQUAL(SERIALIZER_OK, err);
        TEST_ASSERT_EQUAL(36, bytes_written);

        TestStruct2 deserialized_ts2 = {0};
        s_deserialize_options dopts = {
            .format = FORMAT_C_STRUCT,
            .allocator = &g_default_allocator,
        };

        err = s_deserialize(dopts, S_GET_STRUCT_TYPE_INFO(TestStruct2),
                            &deserialized_ts2, buffer, bytes_written);

        TEST_ASSERT_EQUAL(SERIALIZER_OK, err);
        TEST_ASSERT_EQUAL_STRING("Hello, World!",
                                 deserialized_ts2.testStruct_.str_);
        TEST_ASSERT_EQUAL_INT(42, deserialized_ts2.testStruct_.a_);
    }

    {
        TestStruct3 ts3 = {
            .nStaticStructs_ = 3,
            .nDynamicStructs_ = 5,
            .nInts_ = 3,
            .nInts2_ = 2,
            .staticStructs_ =
                {
                    {.str_ = "Hello, World0!", .a_ = 1},
                    {.str_ = "Hello, World1!", .a_ = 2},
                    {.str_ = "Hello, World2!", .a_ = 3},
                },
            .dynamicStructs_ = (TestStruct2*) malloc(3 * sizeof(TestStruct2)),
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

        const s_type_info* info = S_GET_STRUCT_TYPE_INFO(TestStruct3);
        s_serialize_options opts = {0};
        s_serializer_error err =
            s_serialize(opts, S_GET_STRUCT_TYPE_INFO(TestStruct3), &ts3, buffer,
                        sizeof(buffer), &bytes_written);

        TEST_ASSERT_EQUAL(SERIALIZER_OK, err);
        TEST_ASSERT_EQUAL(362, bytes_written);

        TestStruct3 deserialized_ts3 = {0};
        s_deserialize_options dopts = {
            .format = FORMAT_C_STRUCT,
            .allocator = &g_default_allocator,
        };

        err = s_deserialize(dopts, S_GET_STRUCT_TYPE_INFO(TestStruct3),
                            &deserialized_ts3, buffer, bytes_written);

        TEST_ASSERT_EQUAL(SERIALIZER_OK, err);
        TEST_ASSERT_EQUAL_INT(3, deserialized_ts3.nStaticStructs_);
        TEST_ASSERT_EQUAL_INT(5, deserialized_ts3.nDynamicStructs_);
        TEST_ASSERT_EQUAL_INT(3, deserialized_ts3.nInts_);
        TEST_ASSERT_EQUAL_INT(2, deserialized_ts3.nInts2_);
        TEST_ASSERT_EQUAL_STRING("Hello, World0!",
                                 deserialized_ts3.staticStructs_[0].str_);
        TEST_ASSERT_EQUAL_INT(1, deserialized_ts3.staticStructs_[0].a_);
        TEST_ASSERT_EQUAL_STRING("Hello, World1!",
                                 deserialized_ts3.staticStructs_[1].str_);
        TEST_ASSERT_EQUAL_INT(2, deserialized_ts3.staticStructs_[1].a_);
        TEST_ASSERT_EQUAL_STRING("Hello, World2!",
                                 deserialized_ts3.staticStructs_[2].str_);
        TEST_ASSERT_EQUAL_INT(3, deserialized_ts3.staticStructs_[2].a_);

        for (int i = 0; i < deserialized_ts3.nDynamicStructs_; i++) {
            char expected[32];
            snprintf(expected, 32, "Hello, World%d!", i + 3);
            TEST_ASSERT_EQUAL_STRING(
                expected, deserialized_ts3.dynamicStructs_[i].testStruct_.str_);
            TEST_ASSERT_EQUAL_INT(
                i + 4, deserialized_ts3.dynamicStructs_[i].testStruct_.a_);
        }

        for (int i = 0; i < deserialized_ts3.nInts_; i++) {
            TEST_ASSERT_EQUAL_INT(i + 1, deserialized_ts3.ints_[i]);
        }

        for (int i = 0; i < deserialized_ts3.nInts2_; i++) {
            TEST_ASSERT_EQUAL_INT(i + 1, deserialized_ts3.ints2_[i]);
        }

        // deserialize to json for fun
        {
            char deserialized_json[1024] = {0};
            s_deserialize_options dopts = {
                .format = FORMAT_JSON_STRING,
                .allocator = &g_default_allocator,
            };

            err = s_deserialize(dopts, S_GET_STRUCT_TYPE_INFO(TestStruct3),
                                deserialized_json, buffer, bytes_written);

            TEST_ASSERT_EQUAL(SERIALIZER_OK, err);
        }
    }
}

void setUp() {}
void tearDown() {}

int main() {
    UNITY_BEGIN();

    // RUN_TEST(test_serialize_deserialize_simple_struct);
    // RUN_TEST(test_serialize_deserialize_with_empty_and_null_string);
    // RUN_TEST(test_serialize_deserialize_nested_struct);
    // RUN_TEST(test_serialize_deserialize_super_nested_struct);
    RUN_TEST(test_serialize_deserialize_union_structs);
    // RUN_TEST(test_serialize_deserialize_into_json_string);
    // RUN_TEST(test_serialize_deserialize_struct_with_arrays);
    // RUN_TEST(test_serialize_deserialize_arrays_into_json_string);
    // RUN_TEST(tests_seialize_deserialize_struct_with_fixed_strings);

    // RUN_TEST(test_serialize_deserialize_test_structs);

    UNITY_END();
    return 0;
}