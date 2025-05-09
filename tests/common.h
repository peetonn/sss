/*
 * Created on Thu Jan 09 2025
 *
 * Author: Peter Gusev
 * Copyright (c) 2025 Peter Gusev. All rights reserved.
 */

#ifndef __COMMON_H__
#define __COMMON_H__

#include <sss/sss.h>

// system includes
#include <stdbool.h>
#include <stdint.h>

// test structs
// simple struct
typedef struct {
    int32_t id;
    float value;
    bool active;
    const char* name;
    char* passport_number;
    uint8_t blob[32];
} simple_struct;
S_DEFINE_TYPE_INFO(simple_struct);

// nested struct
enum some_enum {
    ENUM_VALUE_1,
    ENUM_VALUE_2,
    ENUM_VALUE_3,
    ENUM_VALUE_4,
};
typedef struct {
    enum some_enum id;
    simple_struct sub;
    const char* name;
} nested_struct;
S_DEFINE_TYPE_INFO(nested_struct);

// nested union
typedef struct {
    char* str;
} str_struct;

typedef struct {
    enum some_enum id;
    union {
        simple_struct sub;
        str_struct str;
        int32_t value;
        char name[32];
    } data;
} nested_union_struct;
S_DEFINE_TYPE_INFO(nested_union_struct);

// struct with builtin arrays
typedef struct {
    int32_t n_static_ints;
    int32_t static_ints[32];
    int32_t n_dynamic_ints;
    int32_t* dynamic_ints;
} builtin_arrays_struct;
S_DEFINE_TYPE_INFO(builtin_arrays_struct);

// struct with struct arrays
typedef struct {
    int32_t n_static_structs;
    simple_struct static_structs[32];
    int32_t n_dynamic_structs;
    simple_struct* dynamic_structs;
} struct_arrays_struct;
S_DEFINE_TYPE_INFO(struct_arrays_struct);

// struct with fixed strings and arrays of fixed strings
typedef struct {
    char name[32];
    int n_phone_numbers;
    char phone_numbers[16][32];
} fixed_strings_struct;
S_DEFINE_TYPE_INFO(fixed_strings_struct);

typedef struct {
    char str_[32];
    int a_;
} TestStruct;
S_DEFINE_TYPE_INFO(TestStruct);

typedef struct {
    TestStruct testStruct_;
} TestStruct2;
S_DEFINE_TYPE_INFO(TestStruct2);

typedef struct {
    int nStaticStructs_;
    int nDynamicStructs_;
    int nInts_;
    int nInts2_;
    TestStruct staticStructs_[16];
    TestStruct2* dynamicStructs_;
    int ints_[16];
    int* ints2_;
} TestStruct3;
S_DEFINE_TYPE_INFO(TestStruct3);

// sample generic "message" struct
enum sss_message_type {
    SSS_MSG_TYPE_CUSTOM_DICT = 0,
    SSS_MSG_TYPE_BLOB,

    SSS_MSG_TYPE_MAX = 0xFF
};

enum sss_generic_value_type {
    SSS_GENERIC_VALUE_TYPE_NONE = 0,
    SSS_GENERIC_VALUE_TYPE_INT32,
    SSS_GENERIC_VALUE_TYPE_DOUBLE,
    SSS_GENERIC_VALUE_TYPE_STRING,

    SSS_GENERIC_VALUE_TYPE_MAX = 0xFF
};

typedef struct {
    enum sss_generic_value_type type_;
    union {
        int int_;
        double double_;
        char string_[32];
    } as_;
} sss_generic_value;
S_DEFINE_TYPE_INFO(sss_generic_value);

typedef struct {
    int n_entries_;
    char keys_[16][32];
    sss_generic_value values_[16];
} sss_custom_dict_data;
S_DEFINE_TYPE_INFO(sss_custom_dict_data);

typedef struct {
    int size_;
    uint8_t* data_;
} sss_blob_data;
S_DEFINE_TYPE_INFO(sss_blob_data);

typedef struct {
    enum sss_message_type type_;
    double timestamp_usec_;
    int seq_no_;

    union {
        sss_custom_dict_data custom_dict_data_;
        sss_blob_data blob_data_;
    } as_;
} sss_system_message;
S_DEFINE_TYPE_INFO(sss_system_message);

#endif