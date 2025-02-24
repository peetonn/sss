/*
 * Created on Wed Jan 08 2025
 *
 * Author: Peter Gusev
 * Copyright (c) 2025 Peter Gusev. All rights reserved.
 */

#ifndef __SERIALIZER_H__
#define __SERIALIZER_H__

#include "sss.h"
#include "tlv.h"

#define MAX_NESTED_LEVELS (32)
#define MAX_TLV_ELEMS (1024)

typedef struct {
    int tlv_el_idx;
    int prev_level;
    const s_type_info* info;
    s_deserialize_options opts;
    uint8_t* data;
    int n_allocations;
    s_serializer_error err;

    struct {
        s_tlv_decoded_element_data el;
        s_type_info* type_info;
        int field_idx;
    } decoded_els[MAX_TLV_ELEMS];

    struct {
        int count;
        char closing_braces[MAX_NESTED_LEVELS][MAX_NESTED_LEVELS];
    } json_context;
} s_deserialize_context;

// helpers
int is_field_present(const void* struct_data, const s_field_info* field);
int is_field_present_ctx(s_deserialize_context* ctx, int field_idx,
                         const s_type_info* field_type_info);

#endif