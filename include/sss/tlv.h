/*
 * Created on Wed Jan 08 2025
 *
 * Author: Peter Gusev
 * Copyright (c) 2025 Peter Gusev. All rights reserved.
 */

#ifndef __TLV_H__
#define __TLV_H__

#include "sss.h"

typedef enum {
    TLV_TAG_FIELD = 0x01,
    TLV_TAG_NESTED,
    TLV_TAG_LIST,
    TLV_TAG_NESTED_LIST,

    TLV_TAG_COMPRESSED_VALUE,
    TLV_TAG_ENCRYPTED_VALUE,

    TLV_TAG_COMPRESSED_NESTED,
    TLV_TAG_ENCRYPTED_NESTED,
} tlv_tag;

s_serializer_error s_tlv_encode(const s_type_info* info, const void* data,
                                uint8_t* buffer, size_t buffer_size,
                                size_t* bytes_written);

typedef struct {
    int idx;
    int level;
    uint16_t type;
    uint32_t length;
    const uint8_t* value;
} s_tlv_decoded_element_data;

typedef void (*s_tlv_element_cb)(
    const s_tlv_decoded_element_data* decoded_el_data, void* user_data);
s_serializer_error s_tlv_decode(const uint8_t* buffer, size_t buffer_size,
                                s_tlv_element_cb cb, void* user_data);

#endif