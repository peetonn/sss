/*
 * Created on Wed Jan 08 2025
 *
 * Author: Peter Gusev
 * Copyright (c) 2025 Peter Gusev. All rights reserved.
 */

#include "sss/serializer.h"

#include "sss/sss.h"
#include "sss/tlv.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

s_serializer_error serialize(s_serialize_options opts, const s_type_info* info,
                             const void* data, uint8_t* buffer,
                             size_t buffer_size, size_t* bytes_written) {
    // TODO: handle compression and encryption
    return s_tlv_encode(info, data, buffer, buffer_size, bytes_written);
}

#define MAX_NESTED_LEVELS (32)

typedef struct {
    int tlv_el_idx;
    int prev_level;
    const s_type_info* info;
    void* user_data;
    s_deserialize_options opts;
    uint8_t* data;
    int n_allocations;
    s_serializer_error err;
} s_deserialize_context;

void tlv_decode_deserializer_cb(
    const s_tlv_decoded_element_data* decoded_el_data, void* user_data) {
    s_deserialize_context* ctx = (s_deserialize_context*) user_data;

    int lvl = 0;
    struct type_info_stack_el {
        const s_field_info* parent_info;
        const s_type_info* type_info;
        int last_field_idx;
        void* data;
    } type_info_stack[MAX_NESTED_LEVELS] = {{NULL, ctx->info, 0, ctx->data}};

    // find field_info for current element
    const s_type_info* type_info = type_info_stack[0].type_info;
    void* data = type_info_stack[0].data;
    const s_field_info* parent_info = NULL;
    const s_field_info* field_info = NULL;
    int field_idx = 0;
    int tlv_idx = 0;

    while (!field_info) {
        if (field_idx < type_info->field_count) {
            // skip optional, non-present fields
            if (!is_field_present(data, &type_info->fields[field_idx])) {
                field_idx++;
                continue;
            }

            if (type_info->fields[field_idx].type == FIELD_TYPE_STRUCT) {
                data = (uint8_t*) data + type_info->fields[field_idx].offset;

                type_info_stack[lvl].last_field_idx = field_idx;
                lvl += 1;

                type_info_stack[lvl].parent_info =
                    &type_info->fields[field_idx];
                type_info_stack[lvl].type_info =
                    type_info->fields[field_idx].struct_type_info;
                type_info_stack[lvl].last_field_idx = 0;
                type_info_stack[lvl].data = data;

                parent_info = type_info_stack[lvl].parent_info;
                type_info = type_info_stack[lvl].type_info;
                field_idx = 0;

                continue;
            } else {
                if (tlv_idx == ctx->tlv_el_idx) {
                    field_info = &type_info->fields[field_idx];
                } else {
                    tlv_idx++;
                    field_idx++;
                }
            }
        } else {
            lvl -= 1;

            if (lvl < 0) {
                ctx->err = SERIALIZER_ERROR_INVALID_TYPE;
                return;
            }

            parent_info = type_info_stack[lvl].parent_info;
            type_info = type_info_stack[lvl].type_info;
            field_idx = type_info_stack[lvl].last_field_idx + 1;
            data = type_info_stack[lvl].data;
        }
    }

    switch (ctx->opts.format) {

    case FORMAT_C_STRUCT: {
        void* dest_ptr = NULL;

        switch (field_info->type) {
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
        case FIELD_TYPE_BLOB: {
            dest_ptr = (uint8_t*) data + field_info->offset;
            memcpy(dest_ptr, decoded_el_data->value, decoded_el_data->length);
        } break;
        case FIELD_TYPE_STRING: {
            // need allocation
            dest_ptr = ctx->opts.allocator->allocate(decoded_el_data->length);

            if (!dest_ptr) {
                if (ctx->err == SERIALIZER_OK)
                    ctx->err = SERIALIZER_ERROR_ALLOCATOR_FAILED;

                return;
            }

            ctx->n_allocations++;
            memset(dest_ptr, 0, decoded_el_data->length);
            memcpy(dest_ptr, decoded_el_data->value, decoded_el_data->length);
            *(char**) ((uint8_t*) data + field_info->offset) = dest_ptr;
        } break;
        case FIELD_TYPE_STRUCT: {
            ctx->err = SERIALIZER_ERROR_INVALID_TYPE;
            assert(0 && "Invalid handling of neted structs");
        } break;

        default:
            ctx->err = SERIALIZER_ERROR_INVALID_TYPE;
        }
    } break;

    case FORMAT_CUSTOM: {
        if (ctx->opts.custom_deserializer) {
            ctx->opts.custom_deserializer(
                decoded_el_data->idx, lvl, decoded_el_data->length,
                decoded_el_data->value, field_info->type, ctx->user_data);
        }
    } break;

    case FORMAT_JSON_STRING: {
        char* json_str_buffer = (char*) ctx->data;

        if (ctx->tlv_el_idx == 0) {
            json_str_buffer[0] = '{';
        }

        if (decoded_el_data->idx == 0 && parent_info) {
            assert(parent_info->type == FIELD_TYPE_STRUCT &&
                   parent_info->struct_type_info);

            // print parent label
            const char* parent_label =
                parent_info->label ? parent_info->label : parent_info->name;

            sprintf(json_str_buffer + strlen(json_str_buffer), "\"%s\":{",
                    parent_label);
        }

        // print field name
        sprintf(json_str_buffer + strlen(json_str_buffer),
                "\"%s\":", field_info->name);

        // print field value
        switch (field_info->type) {
        case FIELD_TYPE_INT8:
        case FIELD_TYPE_UINT8:
        case FIELD_TYPE_INT16:
        case FIELD_TYPE_UINT16:
        case FIELD_TYPE_INT32:
        case FIELD_TYPE_UINT32:
        case FIELD_TYPE_INT64:
        case FIELD_TYPE_UINT64: {
            sprintf(json_str_buffer + strlen(json_str_buffer), "%d",
                    *(int*) decoded_el_data->value);
        } break;

        case FIELD_TYPE_FLOAT:
        case FIELD_TYPE_DOUBLE: {
            sprintf(json_str_buffer + strlen(json_str_buffer), "%f",
                    *(float*) decoded_el_data->value);
        } break;

        case FIELD_TYPE_BOOL: {
            sprintf(json_str_buffer + strlen(json_str_buffer), "%s",
                    *(bool*) decoded_el_data->value ? "true" : "false");
        } break;

        case FIELD_TYPE_BLOB: {
            sprintf(json_str_buffer + strlen(json_str_buffer), "[");
            for (size_t i = 0; i < decoded_el_data->length; i++) {
                if (i != 0)
                    sprintf(json_str_buffer + strlen(json_str_buffer), ",");

                sprintf(json_str_buffer + strlen(json_str_buffer), "%d",
                        ((uint8_t*) decoded_el_data->value)[i]);
            }
            sprintf(json_str_buffer + strlen(json_str_buffer), "]");
        } break;

        case FIELD_TYPE_STRING: {
            sprintf(json_str_buffer + strlen(json_str_buffer), "\"%s\"",
                    (char*) decoded_el_data->value);
        } break;

        default:
            ctx->err = SERIALIZER_ERROR_INVALID_TYPE;
        }

        if (parent_info && parent_info->struct_type_info->field_count ==
                               decoded_el_data->idx + 1) {
            sprintf(json_str_buffer + strlen(json_str_buffer), "}");
        }

        sprintf(json_str_buffer + strlen(json_str_buffer), ",");
    } break;

    default:
        ctx->err = SERIALIZER_ERROR_INVALID_TYPE;
        break;
    }

    ctx->tlv_el_idx++;
    ctx->prev_level = decoded_el_data->level;
}

s_serializer_error deserialize(s_deserialize_options opts,
                               const s_type_info* info, void* data,
                               const uint8_t* buffer, size_t buffer_size) {
    if (!info || !data || !buffer || !opts.allocator) {
        return SERIALIZER_ERROR_INVALID_TYPE;
    }

    s_deserialize_context ctx = {
        .tlv_el_idx = 0,
        .info = info,
        .user_data = data,
        .opts = opts,
        .data = data,
        .n_allocations = 0,
        .err = SERIALIZER_OK,
    };

    s_serializer_error err =
        s_tlv_decode(buffer, buffer_size, tlv_decode_deserializer_cb, &ctx);

    if (err != SERIALIZER_OK || ctx.err != SERIALIZER_OK) {
        // TODO: account for nested structs
        if (ctx.n_allocations) {
            for (int i = 0; i < ctx.info->field_count; i++) {
                const s_field_info* field = &info->fields[i];

                if (field->type == FIELD_TYPE_STRING) {
                    char* str = *(char**) ((uint8_t*) ctx.data + field->offset);
                    if (str) {
                        opts.allocator->deallocate(str);
                    }
                }
            }
        }

        return err;
    }

    // closing bracket handling for json string
    if (opts.format == FORMAT_JSON_STRING) {
        char* json_str_buffer = (char*) ctx.data;
        json_str_buffer[strlen(json_str_buffer) - 1] = '}';
    }

    return SERIALIZER_OK;
}

// helpers
int is_field_present(const void* struct_data, const s_field_info* field) {
    if (field->opts & S_FIELD_OPT_OPTIONAL) {
        void* tag = (void*) ((uint8_t*) struct_data +
                             field->optional_field_info.tag_offset);

        if (field->optional_field_info.tag_type == FIELD_TYPE_INT32) {
            if (*(int32_t*) tag != field->optional_field_info.tag_value_int) {
                return 0;
            }
        } else {
            if (strcmp((char*) tag,
                       field->optional_field_info.tag_value_string) != 0) {
                return 0;
            }
        }
    }

    return 1;
}
