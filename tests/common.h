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

#endif