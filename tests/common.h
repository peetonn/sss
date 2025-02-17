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
};
typedef struct {
    enum some_enum id;
    simple_struct sub;
    const char* name;
} nested_struct;
S_DEFINE_TYPE_INFO(nested_struct);

// nested union
typedef struct {
    enum some_enum id;
    union {
        simple_struct sub;
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

#endif