/*
 * Created on Wed Jan 08 2025
 *
 * Author: Peter Gusev
 * Copyright (c) 2025 Peter Gusev. All rights reserved.
 */

#ifndef __SSS_H__
#define __SSS_H__

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define S_MAX_FIELDS (128)

#ifndef S_TYPE_CHECKING_ENABLED
#define S_TYPE_CHECKING_ENABLED 1
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    FIELD_TYPE_INVALID = 0,

    FIELD_TYPE_INT8,
    FIELD_TYPE_UINT8,
    FIELD_TYPE_INT16,
    FIELD_TYPE_UINT16,
    FIELD_TYPE_INT32,
    FIELD_TYPE_UINT32,
    FIELD_TYPE_INT64,
    FIELD_TYPE_UINT64,

    FIELD_TYPE_FLOAT,
    FIELD_TYPE_DOUBLE,

    FIELD_TYPE_BOOL,
    FIELD_TYPE_BLOB, // continuous memory block

    FIELD_TYPE_STRING, // ANSI string

    FIELD_TYPE_ARRAY, // array of elements

    FIELD_TYPE_STRUCT,
} s_field_type;

typedef enum {
    S_FIELD_OPT_NONE = 0,
    S_FIELD_OPT_OPTIONAL = 1 << 0,
    S_FIELD_OPT_COMPRESSED = 1 << 1,
    S_FIELD_OPT_ENCRYPTED = 1 << 2,
    S_FIELD_OPT_ARRAY_DYNAMIC = 1 << 3,
    S_FIELD_OPT_STRING_FIXED = 1 << 4,

} s_field_opts;

typedef enum {
    S_ARRAY_BUILTIN_TYPE_BLOB,
    S_ARRAY_BUILTIN_TYPE_FLOAT,
    S_ARRAY_BUILTIN_TYPE_STRING,
} s_array_builtin_type;

struct s_type_info;

typedef struct {
    const char* name;
    const char* label;
    s_field_type type;
    size_t offset;
    size_t size;
    int32_t opts;
    struct {
        struct s_type_info* struct_type_info;
        struct {
            s_field_type tag_type;
            size_t tag_offset;
            int tag_value_int;
            const char* tag_value_string;
        } optional_field_info;
        struct {
            size_t size_field_size;
            size_t size_field_offset;
            s_array_builtin_type builtin_type;
        } array_field_info;
    };
} s_field_info;

typedef struct s_type_info {
    const char* type_name;
    s_field_info* fields;
    size_t field_count;
    size_t type_size;
} s_type_info;

typedef enum {
    SERIALIZER_OK = 0,
    SERIALIZER_ERROR_BUFFER_TOO_SMALL = -1,
    SERIALIZER_ERROR_INVALID_TYPE = -2,
    SERIALIZER_ERROR_COMPRESSION_FAILED = -3,
    SERIALIZER_ERROR_ENCRYPTION_FAILED = -4,
    SERIALIZER_ERROR_ALLOCATOR_FAILED = -5,
} s_serializer_error;

typedef void* (*s_allocator_allocate)(size_t, void* user_data);
typedef void (*s_allocator_deallocate)(void*, void* user_data);

typedef struct {
    s_allocator_allocate allocate;
    s_allocator_deallocate deallocate;
} s_allocator;

typedef struct {
    bool is_compressed;         // TODO
    const char* encryption_key; // TODO -- RSA, AES, at minimum
    s_allocator* allocator;     // allocator for compression and encryption
} s_serialize_options;

s_serializer_error s_serialize(s_serialize_options opts,
                               const s_type_info* info, const void* data,
                               uint8_t* buffer, size_t buffer_size,
                               size_t* bytes_written);

// Serialization formats
typedef enum {
    FORMAT_C_STRUCT = 1,
    FORMAT_JSON_STRING = 2,
    FORMAT_CUSTOM = 3,
} s_deserialization_format;

typedef void (*s_custom_deserializer_cb)(int el_idx, int el_lvl, size_t length,
                                         const uint8_t* value,
                                         const s_field_info* field_info,
                                         const s_type_info* type_info,
                                         const s_field_info* parent_info,
                                         void* user_data);

typedef struct {
    s_deserialization_format format;
    s_allocator* allocator; // allocator for deserialized data, decompression
                            // and decryption

    s_custom_deserializer_cb custom_deserializer;
    void* user_data;

    const char* encryption_key; // TODO
} s_deserialize_options;

s_serializer_error s_deserialize(s_deserialize_options opts,
                                 const s_type_info* info, void* data,
                                 const uint8_t* buffer, size_t buffer_size);

#ifdef __cplusplus
}
#endif

// macro API
#define S_GET_STRUCT_TYPE_INFO(TYPE) s_get_struct_type_info_##TYPE()
#define S_DEFINE_TYPE_INFO(TYPE) s_type_info* s_get_struct_type_info_##TYPE()
#define S_SERIALIZE_BEGIN(TYPE)                         \
    s_type_info* s_get_struct_type_info_##TYPE() {      \
        static s_type_info info = {0};                  \
        static bool initialized = false;                \
        if (initialized)                                \
            return &info;                               \
        static s_field_info fields[S_MAX_FIELDS] = {0}; \
        typedef TYPE struct_type;                       \
        struct_type dummy;                              \
        info.type_name = #TYPE;                         \
        info.fields = fields;                           \
        info.field_count = 0;                           \
        info.type_size = sizeof(struct_type);

#define S_SERIALIZE_END() \
    initialized = true;   \
    return &info;         \
    }

#define S_FIELD_LABELED(NAME, TYPE, LABEL)                        \
    assert(info.field_count < S_MAX_FIELDS && "Too many fields"); \
    fields[info.field_count++] =                                  \
        (s_field_info) {.name = #NAME,                            \
                        .label = LABEL,                           \
                        .type = TYPE,                             \
                        .offset = offsetof(struct_type, NAME),    \
                        .size = sizeof(dummy.NAME),               \
                        .opts = S_FIELD_OPT_NONE,                 \
                        .struct_type_info = NULL};

#if S_TYPE_CHECKING_ENABLED
#ifdef __cplusplus
#include <type_traits>
#define S_ASSERT_TYPE(TYPE, VALUE)                                      \
    do {                                                                \
        static_assert(                                                  \
            std::is_same<std::remove_const<TYPE*>::type,                \
                         decltype(const_cast<TYPE*>(&(VALUE)))>::value, \
            "Type mismatch");                                           \
    } while (0)
#else
#define S_ASSERT_TYPE(TYPE, VALUE) ((TYPE*) {0} = &(VALUE))
#endif
#else
#define S_ASSERT_TYPE(TYPE, VALUE)
#endif

#define GET_MACRO(_1, _2, NAME, ...) NAME
#define GET_MACRO_3(_1, _2, _3, NAME, ...) NAME
#define GET_MACRO_4(_1, _2, _3, _4, NAME, ...) NAME

#define S_FIELD_INT8_LABELED(NAME, FIELD_LABEL, ...) \
    S_ASSERT_TYPE(int8_t, dummy.NAME);               \
    S_FIELD_LABELED(NAME, FIELD_TYPE_INT8, FIELD_LABEL);
#define S_FIELD_INT8(...)                        \
    GET_MACRO(__VA_ARGS__, S_FIELD_INT8_LABELED, \
              S_FIELD_INT8_LABELED)(__VA_ARGS__, NULL)

#define S_FIELD_UINT8_LABELED(NAME, FIELD_LABEL, ...) \
    S_ASSERT_TYPE(uint8_t, dummy.NAME);               \
    S_FIELD_LABELED(NAME, FIELD_TYPE_UINT8, FIELD_LABEL);
#define S_FIELD_UINT8(...)                        \
    GET_MACRO(__VA_ARGS__, S_FIELD_UINT8_LABELED, \
              S_FIELD_UINT8_LABELED)(__VA_ARGS__, NULL)

#define S_FIELD_INT16_LABELED(NAME, FIELD_LABEL, ...) \
    S_ASSERT_TYPE(int16_t, dummy.NAME);               \
    S_FIELD_LABELED(NAME, FIELD_TYPE_INT16, FIELD_LABEL);
#define S_FIELD_INT16(...)                        \
    GET_MACRO(__VA_ARGS__, S_FIELD_INT16_LABELED, \
              S_FIELD_INT16_LABELED)(__VA_ARGS__, NULL)

#define S_FIELD_UINT16_LABELED(NAME, FIELD_LABEL, ...) \
    S_ASSERT_TYPE(uint16_t, dummy.NAME);               \
    S_FIELD_LABELED(NAME, FIELD_TYPE_UINT16, FIELD_LABEL);
#define S_FIELD_UINT16(...)                        \
    GET_MACRO(__VA_ARGS__, S_FIELD_UINT16_LABELED, \
              S_FIELD_UINT16_LABELED)(__VA_ARGS__, NULL)

#define S_FIELD_INT32_LABELED(NAME, FIELD_LABEL, ...) \
    S_ASSERT_TYPE(int32_t, dummy.NAME);               \
    S_FIELD_LABELED(NAME, FIELD_TYPE_INT32, FIELD_LABEL);
#define S_FIELD_INT32(...)                        \
    GET_MACRO(__VA_ARGS__, S_FIELD_INT32_LABELED, \
              S_FIELD_INT32_LABELED)(__VA_ARGS__, NULL)

// special case for enums without type check
#define S_FIELD_ENUM_LABELED(NAME, FIELD_LABEL, ...) \
    S_FIELD_LABELED(NAME, FIELD_TYPE_INT32, FIELD_LABEL);
#define S_FIELD_ENUM(...)                                              \
    GET_MACRO(__VA_ARGS__, S_FIELD_ENUM_LABELED, S_FIELD_ENUM_LABELED) \
    (__VA_ARGS__, NULL)

#define S_FIELD_UINT32_LABELED(NAME, FIELD_LABEL, ...) \
    S_ASSERT_TYPE(uint32_t, dummy.NAME);               \
    S_FIELD_LABELED(NAME, FIELD_TYPE_UINT32, FIELD_LABEL);
#define S_FIELD_UINT32(...)                        \
    GET_MACRO(__VA_ARGS__, S_FIELD_UINT32_LABELED, \
              S_FIELD_UINT32_LABELED)(__VA_ARGS__, NULL)

#define S_FIELD_INT64_LABELED(NAME, FIELD_LABEL, ...) \
    S_ASSERT_TYPE(int64_t, dummy.NAME);               \
    S_FIELD_LABELED(NAME, FIELD_TYPE_INT64, FIELD_LABEL);
#define S_FIELD_INT64(...)                        \
    GET_MACRO(__VA_ARGS__, S_FIELD_INT64_LABELED, \
              S_FIELD_INT64_LABELED)(__VA_ARGS__, NULL)

#define S_FIELD_UINT64_LABELED(NAME, FIELD_LABEL, ...) \
    S_ASSERT_TYPE(uint64_t, dummy.NAME);               \
    S_FIELD_LABELED(NAME, FIELD_TYPE_UINT64, FIELD_LABEL);
#define S_FIELD_UINT64(...)                        \
    GET_MACRO(__VA_ARGS__, S_FIELD_UINT64_LABELED, \
              S_FIELD_UINT64_LABELED)(__VA_ARGS__, NULL)

#define S_FIELD_FLOAT_LABELED(NAME, FIELD_LABEL, ...) \
    S_ASSERT_TYPE(float, dummy.NAME);                 \
    S_FIELD_LABELED(NAME, FIELD_TYPE_FLOAT, FIELD_LABEL);
#define S_FIELD_FLOAT(...)                        \
    GET_MACRO(__VA_ARGS__, S_FIELD_FLOAT_LABELED, \
              S_FIELD_FLOAT_LABELED)(__VA_ARGS__, NULL)

#define S_FIELD_DOUBLE_LABELED(NAME, FIELD_LABEL, ...) \
    S_ASSERT_TYPE(double, dummy.NAME);                 \
    S_FIELD_LABELED(NAME, FIELD_TYPE_DOUBLE, FIELD_LABEL);
#define S_FIELD_DOUBLE(...)                        \
    GET_MACRO(__VA_ARGS__, S_FIELD_DOUBLE_LABELED, \
              S_FIELD_DOUBLE_LABELED)(__VA_ARGS__, NULL)

#define S_FIELD_STRING_LABELED(NAME, FIELD_LABEL, ...) \
    S_ASSERT_TYPE(char*, dummy.NAME);                  \
    S_FIELD_LABELED(NAME, FIELD_TYPE_STRING, FIELD_LABEL);
#define S_FIELD_STRING(...)                        \
    GET_MACRO(__VA_ARGS__, S_FIELD_STRING_LABELED, \
              S_FIELD_STRING_LABELED)(__VA_ARGS__, NULL)

#define S_FIELD_STRING_CONST_LABELED(NAME, FIELD_LABEL, ...) \
    S_ASSERT_TYPE(const char*, dummy.NAME);                  \
    S_FIELD_LABELED(NAME, FIELD_TYPE_STRING, FIELD_LABEL);
#define S_FIELD_STRING_CONST(...)                        \
    GET_MACRO(__VA_ARGS__, S_FIELD_STRING_CONST_LABELED, \
              S_FIELD_STRING_CONST_LABELED)(__VA_ARGS__, NULL)

#define S_FIELD_STRING_FIXED_LABELED(NAME, FIELD_LABEL, ...) \
    S_FIELD_LABELED(NAME, FIELD_TYPE_STRING, FIELD_LABEL);   \
    fields[info.field_count - 1].opts |= S_FIELD_OPT_STRING_FIXED;
#define S_FIELD_STRING_FIXED(...)                        \
    GET_MACRO(__VA_ARGS__, S_FIELD_STRING_FIXED_LABELED, \
              S_FIELD_STRING_FIXED_LABELED)(__VA_ARGS__, NULL)

#define S_FIELD_BOOL_LABELED(NAME, FIELD_LABEL, ...) \
    S_ASSERT_TYPE(bool, dummy.NAME);                 \
    S_FIELD_LABELED(NAME, FIELD_TYPE_BOOL, FIELD_LABEL);
#define S_FIELD_BOOL(...)                        \
    GET_MACRO(__VA_ARGS__, S_FIELD_BOOL_LABELED, \
              S_FIELD_BOOL_LABELED)(__VA_ARGS__, NULL)

#define S_FIELD_BLOB_LABELED(NAME, FIELD_LABEL, ...) \
    S_FIELD_LABELED(NAME, FIELD_TYPE_BLOB, FIELD_LABEL);

#define S_FIELD_BLOB(...)                        \
    GET_MACRO(__VA_ARGS__, S_FIELD_BLOB_LABELED, \
              S_FIELD_BLOB_LABELED)(__VA_ARGS__, NULL)

#define S_FIELD_STRUCT_LABELED(NAME, TYPE, FIELD_LABEL, ...)      \
    assert(info.field_count < S_MAX_FIELDS && "Too many fields"); \
    fields[info.field_count++] =                                  \
        (s_field_info) {.name = #NAME,                            \
                        .label = FIELD_LABEL,                     \
                        .type = FIELD_TYPE_STRUCT,                \
                        .offset = offsetof(struct_type, NAME),    \
                        .size = sizeof(dummy.NAME),               \
                        .opts = S_FIELD_OPT_NONE,                 \
                        .struct_type_info = S_GET_STRUCT_TYPE_INFO(TYPE)};

#define S_FIELD_STRUCT(...)                          \
    GET_MACRO_3(__VA_ARGS__, S_FIELD_STRUCT_LABELED, \
                S_FIELD_STRUCT_LABELED)(__VA_ARGS__, NULL)

#define S_UNION_BEGIN_TAG(NAME, TAG_NAME)                  \
    {                                                      \
        size_t union_start_field_index = info.field_count; \
        size_t tag_offset = offsetof(struct_type, TAG_NAME);

#define S_UNION_END()                                                     \
    for (size_t i = union_start_field_index; i < info.field_count; i++) { \
        info.fields[i].opts |= S_FIELD_OPT_OPTIONAL;                      \
    }                                                                     \
    }

#define S_FIELD_TAGGED_INT(NAME, TAG_VALUE)                                   \
    {                                                                         \
        bool field_found = false;                                             \
        for (size_t i = union_start_field_index; i < info.field_count; i++) { \
            if (strcmp(info.fields[i].name, #NAME) == 0) {                    \
                field_found = true;                                           \
                info.fields[i].opts |= S_FIELD_OPT_OPTIONAL;                  \
                info.fields[i].optional_field_info.tag_offset = tag_offset;   \
                assert(info.fields[i].offset > tag_offset &&                  \
                       "Tag field must be before tagged field");              \
                info.fields[i].optional_field_info.tag_value_int = TAG_VALUE; \
                info.fields[i].optional_field_info.tag_type =                 \
                    FIELD_TYPE_INT32;                                         \
                break;                                                        \
            }                                                                 \
        }                                                                     \
        assert(field_found && "Field " #NAME " not found in union");          \
    }

#define S_FIELD_TAGGED_STRING(NAME, TAG_VALUE)                                \
    {                                                                         \
        bool field_found = false;                                             \
        for (size_t i = union_start_field_index; i < info.field_count; i++) { \
            if (strcmp(info.fields[i].name, #NAME) == 0) {                    \
                field_found = true;                                           \
                info.fields[i].opts |= S_FIELD_OPT_OPTIONAL;                  \
                info.fields[i].optional_field_info.tag_offset = tag_offset;   \
                assert(info.fields[i].offset > tag_offset &&                  \
                       "Tag field must be before tagged field");              \
                info.fields[i].optional_field_info.tag_value_string =         \
                    TAG_VALUE;                                                \
                info.fields[i].optional_field_info.tag_type =                 \
                    FIELD_TYPE_STRING;                                        \
                break;                                                        \
            }                                                                 \
        }                                                                     \
        assert(field_found && "Field " #NAME " not found in union");          \
    }

#define S_BUILTIN_ARRAY_FIELD_SET_FLOAT(NAME)                        \
    {                                                                \
        bool field_found = false;                                    \
        for (size_t i = 0; i < info.field_count; i++) {              \
            if (strcmp(info.fields[i].name, #NAME) == 0) {           \
                field_found = true;                                  \
                info.fields[i].array_field_info.builtin_type =       \
                    S_ARRAY_BUILTIN_TYPE_FLOAT;                      \
                break;                                               \
            }                                                        \
        }                                                            \
        assert(field_found && "Field " #NAME " not found in union"); \
    }

#define S_BUILTIN_ARRAY_FIELD_SET_TYPE(NAME, BUILTIN_TYPE)                   \
    {                                                                        \
        bool field_found = false;                                            \
        for (size_t i = 0; i < info.field_count; i++) {                      \
            if (strcmp(info.fields[i].name, #NAME) == 0) {                   \
                field_found = true;                                          \
                info.fields[i].array_field_info.builtin_type = BUILTIN_TYPE; \
                break;                                                       \
            }                                                                \
        }                                                                    \
        assert(field_found && "Field " #NAME " not found in union");         \
    }

#define S_FIELD_ARRAY_STATIC_LABELED(NAME, SIZE, FIELD_LABEL, ...)    \
    S_FIELD_LABELED(NAME, FIELD_TYPE_ARRAY, FIELD_LABEL);             \
    fields[info.field_count - 1].size = sizeof(dummy.NAME[0]);        \
    fields[info.field_count - 1].array_field_info.size_field_size =   \
        sizeof(dummy.SIZE);                                           \
    fields[info.field_count - 1].array_field_info.size_field_offset = \
        offsetof(struct_type, SIZE);                                  \
    fields[info.field_count - 1].array_field_info.builtin_type =      \
        S_ARRAY_BUILTIN_TYPE_BLOB;
#define S_FIELD_ARRAY_STATIC(...)                          \
    GET_MACRO_3(__VA_ARGS__, S_FIELD_ARRAY_STATIC_LABELED, \
                S_FIELD_ARRAY_STATIC_LABELED)(__VA_ARGS__, NULL)

#define S_FIELD_ARRAY_DYNAMIC_LABELED(NAME, SIZE, FIELD_LABEL, ...)   \
    S_FIELD_LABELED(NAME, FIELD_TYPE_ARRAY, FIELD_LABEL);             \
    fields[info.field_count - 1].size = sizeof(*dummy.NAME);          \
    fields[info.field_count - 1].opts |= S_FIELD_OPT_ARRAY_DYNAMIC;   \
    fields[info.field_count - 1].array_field_info.size_field_size =   \
        sizeof(dummy.SIZE);                                           \
    fields[info.field_count - 1].array_field_info.size_field_offset = \
        offsetof(struct_type, SIZE);                                  \
    fields[info.field_count - 1].array_field_info.builtin_type =      \
        S_ARRAY_BUILTIN_TYPE_BLOB;
#define S_FIELD_ARRAY_DYNAMIC(...)                          \
    GET_MACRO_3(__VA_ARGS__, S_FIELD_ARRAY_DYNAMIC_LABELED, \
                S_FIELD_ARRAY_DYNAMIC_LABELED)(__VA_ARGS__, NULL)

#define S_FIELD_STRUCT_ARRAY_STATIC_LABELED(NAME, SIZE, TYPE, FIELD_LABEL, \
                                            ...)                           \
    assert(info.field_count < S_MAX_FIELDS && "Too many fields");          \
    fields[info.field_count++] =                                           \
        (s_field_info) {.name = #NAME,                                     \
                        .label = FIELD_LABEL,                              \
                        .type = FIELD_TYPE_ARRAY,                          \
                        .offset = offsetof(struct_type, NAME),             \
                        .size = sizeof(dummy.NAME[0]),                     \
                        .opts = S_FIELD_OPT_NONE,                          \
                        .struct_type_info = S_GET_STRUCT_TYPE_INFO(TYPE)}; \
    fields[info.field_count - 1].array_field_info.size_field_size =        \
        sizeof(dummy.SIZE);                                                \
    fields[info.field_count - 1].array_field_info.size_field_offset =      \
        offsetof(struct_type, SIZE);
#define S_FIELD_STRUCT_ARRAY_STATIC(...)                          \
    GET_MACRO_4(__VA_ARGS__, S_FIELD_STRUCT_ARRAY_STATIC_LABELED, \
                S_FIELD_STRUCT_ARRAY_STATIC_LABELED)(__VA_ARGS__, NULL)

#define S_FIELD_STRUCT_ARRAY_DYNAMIC_LABELED(NAME, SIZE, TYPE, FIELD_LABEL, \
                                             ...)                           \
    assert(info.field_count < S_MAX_FIELDS && "Too many fields");           \
    fields[info.field_count++] =                                            \
        (s_field_info) {.name = #NAME,                                      \
                        .label = FIELD_LABEL,                               \
                        .type = FIELD_TYPE_ARRAY,                           \
                        .offset = offsetof(struct_type, NAME),              \
                        .size = sizeof(*dummy.NAME),                        \
                        .opts = S_FIELD_OPT_ARRAY_DYNAMIC,                  \
                        .struct_type_info = S_GET_STRUCT_TYPE_INFO(TYPE)};  \
    fields[info.field_count - 1].array_field_info.size_field_size =         \
        sizeof(dummy.SIZE);                                                 \
    fields[info.field_count - 1].array_field_info.size_field_offset =       \
        offsetof(struct_type, SIZE);
#define S_FIELD_STRUCT_ARRAY_DYNAMIC(...)                          \
    GET_MACRO_4(__VA_ARGS__, S_FIELD_STRUCT_ARRAY_DYNAMIC_LABELED, \
                S_FIELD_STRUCT_ARRAY_DYNAMIC_LABELED)(__VA_ARGS__, NULL)
#endif