/*
 * Created on Wed Jan 08 2025
 *
 * Author: Peter Gusev
 * Copyright (c) 2025 Peter Gusev. All rights reserved.
 */

#include "sss/tlv.h"

#include "sss/log.h"
#include "sss/serializer.h"

// system includes
#include <arpa/inet.h>

#pragma pack(push, 1)
typedef struct {
    uint16_t tag;
    uint32_t length;
    uint8_t value[];
} s_tlv_element;
#pragma pack(pop)

static s_tlv_element k_dummy_el; // helper struct for macro

#define TLV_SIZEOF_T (sizeof(k_dummy_el.tag))
#define TLV_SIZEOF_L (sizeof(k_dummy_el.length))
#define TLV_SIZEOF_TL (TLV_SIZEOF_T + TLV_SIZEOF_L)
#define TLV_SIZEOF(el) ((size_t) (TLV_SIZEOF_TL + (size_t) (el)->length))
#define TLV_AS_T(buffer) (((uint16_t*) buffer)[0])
#define TLV_AS_L(buffer) (((uint32_t*) (buffer + TLV_SIZEOF_T))[0])
#define TLV_AS_V(buffer) (buffer + TLV_SIZEOF_TL)

s_serializer_error s_tlv_decode_level(int level, const uint8_t* buffer,
                                      size_t buffer_size, s_tlv_element_cb cb,
                                      void* user_data);

s_serializer_error s_tlv_encode_field(const s_field_info* field,
                                      const void* data, uint8_t* tlv_buffer,
                                      size_t buffer_size,
                                      size_t* bytes_written) {
    const uint8_t* field_data = (const uint8_t*) data + field->offset;

    // check if field is optional and if it should be serialized
    if (is_field_present(data, field) == 0) {
        *bytes_written = 0;
        return SERIALIZER_OK;
    }

    const void* value_ptr = NULL;
    s_tlv_element tlv_el = {0};

    switch (field->type) {
    case FIELD_TYPE_INT8:
    case FIELD_TYPE_UINT8:
    case FIELD_TYPE_INT16:
    case FIELD_TYPE_UINT16:
    case FIELD_TYPE_INT32:
    case FIELD_TYPE_UINT32:
    case FIELD_TYPE_INT64:
    case FIELD_TYPE_UINT64:
    case FIELD_TYPE_FLOAT:
    case FIELD_TYPE_DOUBLE:
    case FIELD_TYPE_BOOL:
    case FIELD_TYPE_BLOB:
    case FIELD_TYPE_STRING: {
        if (field->type == FIELD_TYPE_STRING) {
            // include null terminator for strings
            char* str;
            if (field->opts & S_FIELD_OPT_STRING_FIXED) {
                str = (char*) field_data;
            } else {
                memcpy(&str, field_data, sizeof(char*));
            }

            if (str) {
                tlv_el.length = (uint32_t) strlen(str) + 1;
                value_ptr = str;
            } else {
                tlv_el.length = 0;
                value_ptr = NULL;
            }
        } else {
            tlv_el.length = (uint32_t) field->size;
            value_ptr = field_data;
        }

        tlv_el.tag = (uint16_t) TLV_TAG_FIELD;
    } break;

    case FIELD_TYPE_STRUCT: {
        const s_type_info* sub_info = field->struct_type_info;
        if (!sub_info) {
            return SERIALIZER_ERROR_INVALID_TYPE;
        }

        tlv_el.tag = (uint16_t) TLV_TAG_NESTED;
        value_ptr = NULL;

        if (buffer_size < TLV_SIZEOF_TL) {
            return SERIALIZER_ERROR_BUFFER_TOO_SMALL;
        }

        // encode directly into buffer
        size_t sub_bytes_written = 0;
        s_serializer_error err =
            s_tlv_encode(sub_info, field_data, tlv_buffer + TLV_SIZEOF_TL,
                         buffer_size - TLV_SIZEOF_TL, &sub_bytes_written);

        if (err != SERIALIZER_OK) {
            return err;
        }

        tlv_el.length = (uint32_t) sub_bytes_written;
    } break;

    case FIELD_TYPE_ARRAY: {
        uint32_t array_size = 0;
        const uint8_t* size_field_data =
            (const uint8_t*) data + field->array_field_info.size_field_offset;

        switch (field->array_field_info.size_field_size) {
        case 1: {
            array_size = (uint32_t) *(uint8_t*) size_field_data;
        } break;
        case 2: {
            array_size = (uint32_t) (*(uint16_t*) size_field_data);
        } break;
        case 4:
        case 8: {
            array_size = (uint32_t) (*(uint32_t*) size_field_data);
        } break;
        default:
            return SERIALIZER_ERROR_INVALID_TYPE;
        }

        bool is_dynamic = field->opts & S_FIELD_OPT_ARRAY_DYNAMIC;
        bool is_struct_array = field->struct_type_info;

        tlv_el.tag = is_struct_array ? TLV_TAG_NESTED_LIST : TLV_TAG_LIST;

        if (!is_struct_array) {
            if (field->array_field_info.builtin_type ==
                S_ARRAY_BUILTIN_TYPE_STRING) {
                int tlv_length = 0;

                // copy strings one by one, include null terminator
                for (int i = 0; i < array_size; ++i) {
                    char* str = (char*) field_data + field->size * i;
                    int str_len = strlen(str) + 1;

                    // copy
                    if (buffer_size < str_len) {
                        return SERIALIZER_ERROR_BUFFER_TOO_SMALL;
                    }

                    memcpy(tlv_buffer + TLV_SIZEOF_TL + tlv_length, str,
                           str_len);
                    tlv_length += str_len;
                    buffer_size -= str_len;
                }

                tlv_el.length = tlv_length;
                value_ptr = NULL;
            } else { // just serialize as a blob
                tlv_el.length = array_size * field->size;
                value_ptr =
                    is_dynamic ? *(const void**) field_data : field_data;
            }
        } else {
            const s_type_info* sub_info = field->struct_type_info;

            value_ptr = NULL;

            if (buffer_size < TLV_SIZEOF_TL) {
                return SERIALIZER_ERROR_BUFFER_TOO_SMALL;
            }

            // encode structs directly into buffer
            size_t sub_bytes_written = 0;

            for (uint32_t i = 0; i < array_size; i++) {
                const void* array_element_data =
                    is_dynamic ? *(const void**) field_data + field->size * i
                               : field_data + field->size * i;
                size_t bytes_written = 0;
                s_serializer_error err = s_tlv_encode(
                    sub_info, array_element_data,
                    tlv_buffer + TLV_SIZEOF_TL + sub_bytes_written,
                    buffer_size - TLV_SIZEOF_TL - sub_bytes_written,
                    &bytes_written);

                if (err != SERIALIZER_OK) {
                    return err;
                }

                sub_bytes_written += bytes_written;
            }

            tlv_el.length = (uint32_t) sub_bytes_written;
        }
    } break;

    default:
        return SERIALIZER_ERROR_INVALID_TYPE;
    }

    if (buffer_size < TLV_SIZEOF(&tlv_el)) {
        return SERIALIZER_ERROR_BUFFER_TOO_SMALL;
    }

    // copy data
    uint16_t type_net = htons(tlv_el.tag);
    uint32_t length_net = htonl(tlv_el.length);
    memcpy(tlv_buffer, &type_net, TLV_SIZEOF_T);
    memcpy(tlv_buffer + TLV_SIZEOF_T, &length_net, TLV_SIZEOF_L);

    if (value_ptr) {
        memcpy(TLV_AS_V(tlv_buffer), value_ptr, tlv_el.length);
    }

    *bytes_written = TLV_SIZEOF(&tlv_el);

    return SERIALIZER_OK;
}

s_serializer_error s_tlv_encode(const s_type_info* info, const void* data,
                                uint8_t* buffer, size_t buffer_size,
                                size_t* bytes_written) {
    if (!info || !data || !buffer || !bytes_written) {
        return SERIALIZER_ERROR_INVALID_TYPE;
    }

    size_t total_bytes = 0;
    uint8_t* current_buffer = buffer;
    size_t remaining_size = buffer_size;

    for (size_t i = 0; i < info->field_count; i++) {
        size_t field_bytes = 0;
        s_serializer_error err =
            s_tlv_encode_field(&info->fields[i], data, current_buffer,
                               remaining_size, &field_bytes);

        if (err != SERIALIZER_OK) {
            return err;
        }

        current_buffer += field_bytes;
        remaining_size -= field_bytes;
        total_bytes += field_bytes;
    }

    *bytes_written = total_bytes;
    return SERIALIZER_OK;
}

s_serializer_error s_tlv_decode_element(int idx, int level,
                                        const s_tlv_element* tlv_el,
                                        s_tlv_element_cb cb, void* user_data) {

    uint16_t tag = ntohs(tlv_el->tag);
    uint32_t length = ntohl(tlv_el->length);

    switch (tag) {
    case TLV_TAG_FIELD:
    case TLV_TAG_LIST: {
        s_tlv_decoded_element_data decoded_el_data = {
            .idx = idx,
            .level = level,
            .type = tag,
            .length = length,
            .value = tlv_el->value,
        };

        cb(&decoded_el_data, user_data);
    } break;

    case TLV_TAG_NESTED_LIST: {
        s_tlv_decode_level(level + 1, tlv_el->value, length, cb, user_data);
    } break;

    case TLV_TAG_NESTED: {
        s_tlv_decode_level(level + 1, tlv_el->value, length, cb, user_data);
    } break;

    default:
        return SERIALIZER_ERROR_INVALID_TYPE;
    }

    return SERIALIZER_OK;
}

s_serializer_error s_tlv_decode_level(int level, const uint8_t* buffer,
                                      size_t buffer_size, s_tlv_element_cb cb,
                                      void* user_data) {

    LOG_DEBUG("s_tlv_decode_level %d buf sz %d", level, buffer_size);

    if (!buffer || !cb) {
        return SERIALIZER_ERROR_INVALID_TYPE;
    }

    const uint8_t* tlv_buffer = buffer;
    size_t remaining_size = buffer_size;
    int idx = 0;

    while (remaining_size > 0) {
        const s_tlv_element* tlv_el = (const s_tlv_element*) tlv_buffer;
        uint32_t tlv_el_length = ntohl(tlv_el->length);

        LOG_DEBUG("s_tlv_decode_level el %d %d tag %d len %d", idx, level,
                  ntohs(tlv_el->tag), tlv_el_length);

        // sanity check
        if (remaining_size < TLV_SIZEOF_TL + tlv_el_length) {
            return SERIALIZER_ERROR_INVALID_TYPE;
        }

        s_serializer_error err =
            s_tlv_decode_element(idx, level, tlv_el, cb, user_data);

        // if uknown tag - silently ignore
        if (!(err == SERIALIZER_OK || err == SERIALIZER_ERROR_INVALID_TYPE)) {
            return err;
        }

        tlv_buffer += (TLV_SIZEOF_TL + tlv_el_length);
        remaining_size -= (TLV_SIZEOF_TL + tlv_el_length);
        idx += 1;
    }

    return SERIALIZER_OK;
}

s_serializer_error s_tlv_decode(const uint8_t* buffer, size_t buffer_size,
                                s_tlv_element_cb cb, void* user_data) {
    LOG_DEBUG("s_tlv_decode");

    return s_tlv_decode_level(0, buffer, buffer_size, cb, user_data);
}
