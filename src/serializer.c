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

s_serializer_error s_serialize(s_serialize_options opts,
                               const s_type_info* info, const void* data,
                               uint8_t* buffer, size_t buffer_size,
                               size_t* bytes_written) {
    // TODO: handle compression and encryption
    return s_tlv_encode(info, data, buffer, buffer_size, bytes_written);
}

#define MAX_NESTED_LEVELS (32)

typedef struct {
    int tlv_el_idx;
    int prev_level;
    const s_type_info* info;
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
        uint32_t pending_array_el_count;
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

            switch (type_info->fields[field_idx].type) {
            case FIELD_TYPE_STRUCT: {
                data = (uint8_t*) data + type_info->fields[field_idx].offset;

                type_info_stack[lvl].last_field_idx = field_idx;
                lvl += 1;

                type_info_stack[lvl].parent_info =
                    &type_info->fields[field_idx];
                type_info_stack[lvl].type_info =
                    type_info->fields[field_idx].struct_type_info;
                type_info_stack[lvl].last_field_idx = 0;
                type_info_stack[lvl].data = data;
                type_info_stack[lvl].pending_array_el_count = 0;

                parent_info = type_info_stack[lvl].parent_info;
                type_info = type_info_stack[lvl].type_info;
                field_idx = 0;

                continue;
            } break;
            case FIELD_TYPE_ARRAY: { // only handle struct arrays, otherwise --
                                     // fall through
                bool is_struct_array =
                    type_info->fields[field_idx].struct_type_info;
                bool is_dynamic = type_info->fields[field_idx].opts &
                                  S_FIELD_OPT_ARRAY_DYNAMIC;

                if (is_struct_array) {
                    uint32_t array_size = 0;
                    const uint8_t* size_field_data =
                        (const uint8_t*) data +
                        type_info->fields[field_idx]
                            .array_field_info.size_field_offset;

                    switch (type_info->fields[field_idx]
                                .array_field_info.size_field_size) {
                    case 1: {
                        array_size = (uint32_t) *(uint8_t*) size_field_data;
                    } break;
                    case 2: {
                        array_size = (uint32_t) *(uint16_t*) size_field_data;
                    } break;
                    case 4:
                    case 8: {
                        array_size = *(uint32_t*) size_field_data;
                    } break;
                    default:
                        ctx->err = SERIALIZER_ERROR_INVALID_TYPE;
                        return;
                    }

                    if (is_dynamic) {
                        void** array_data_ptr =
                            (void**) ((uint8_t*) data +
                                      type_info->fields[field_idx].offset);

                        if (!*array_data_ptr && array_size) {
                            *array_data_ptr = ctx->opts.allocator->allocate(
                                array_size * type_info->fields[field_idx]
                                                 .struct_type_info->type_size,
                                ctx->opts.user_data);
                        }

                        if (!*array_data_ptr) {
                            ctx->err = SERIALIZER_ERROR_ALLOCATOR_FAILED;
                            return;
                        }

                        data = *array_data_ptr;
                    } else {
                        data = (uint8_t*) data +
                               type_info->fields[field_idx].offset;
                    }

                    type_info_stack[lvl].last_field_idx = field_idx;
                    lvl += 1;

                    // push array field info
                    type_info_stack[lvl].parent_info =
                        &type_info->fields[field_idx];
                    type_info_stack[lvl].type_info =
                        type_info->fields[field_idx].struct_type_info;
                    type_info_stack[lvl].last_field_idx = 0;
                    type_info_stack[lvl].data = data;
                    type_info_stack[lvl].pending_array_el_count = array_size;

                    parent_info = type_info_stack[lvl].parent_info;
                    type_info = type_info_stack[lvl].type_info;
                    field_idx = 0;

                    continue;
                    // break;
                }
            } // fallthrough for non-struct arrays
            default: {
                if (tlv_idx == ctx->tlv_el_idx) {
                    field_info = &type_info->fields[field_idx];
                } else {
                    tlv_idx++;
                    field_idx++;
                }
            } break;
            }
        } else {
            // if we're iterating array, check if we're not done yet
            if (type_info_stack[lvl].pending_array_el_count > 1) {
                type_info_stack[lvl].pending_array_el_count -= 1;
                // shift data pointer to next element
                data += type_info_stack[lvl].type_info->type_size;
                field_idx = 0;
                continue;
            }

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
            if (field_info->opts & S_FIELD_OPT_STRING_FIXED) {
                dest_ptr = (uint8_t*) data + field_info->offset;
                memcpy(dest_ptr, decoded_el_data->value,
                       decoded_el_data->length);
            } else {
                // need allocation
                if (decoded_el_data->length) {
                    dest_ptr = ctx->opts.allocator->allocate(
                        decoded_el_data->length, ctx->opts.user_data);

                    if (!dest_ptr) {
                        if (ctx->err == SERIALIZER_OK)
                            ctx->err = SERIALIZER_ERROR_ALLOCATOR_FAILED;

                        return;
                    }

                    ctx->n_allocations++;
                    memset(dest_ptr, 0, decoded_el_data->length);
                    memcpy(dest_ptr, decoded_el_data->value,
                           decoded_el_data->length);
                }

                *(char**) ((uint8_t*) data + field_info->offset) = dest_ptr;
            }
        } break;
        case FIELD_TYPE_ARRAY: {
            // struct arrays are handled in the beginning of the function
            if (field_info->struct_type_info) {
                ctx->err = SERIALIZER_ERROR_INVALID_TYPE;
                return;
            }

            if (field_info->opts & S_FIELD_OPT_ARRAY_DYNAMIC) {
                dest_ptr = ctx->opts.allocator->allocate(
                    decoded_el_data->length, ctx->opts.user_data);

                if (!dest_ptr) {
                    if (ctx->err == SERIALIZER_OK)
                        ctx->err = SERIALIZER_ERROR_ALLOCATOR_FAILED;

                    return;
                }

                ctx->n_allocations++;
                memcpy(dest_ptr, decoded_el_data->value,
                       decoded_el_data->length);

                *(void**) ((uint8_t*) data + field_info->offset) = dest_ptr;
            } else {
                if (field_info->array_field_info.builtin_type &
                    S_ARRAY_BUILTIN_TYPE_STRING) {
                    dest_ptr = (uint8_t*) data + field_info->offset;
                    // unpack strings - separated by null terminators
                    int offset = 0;

                    while (offset < decoded_el_data->length) {
                        char* str = (char*) decoded_el_data->value + offset;
                        int str_len = strlen(str) + 1;

                        memcpy(dest_ptr, str, str_len);
                        dest_ptr += field_info->size;
                        offset += str_len;
                    }
                } else {
                    dest_ptr = (uint8_t*) data + field_info->offset;
                    memcpy(dest_ptr, decoded_el_data->value,
                           decoded_el_data->length);
                }
            }
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
            ctx->opts.custom_deserializer(decoded_el_data->idx, lvl,
                                          decoded_el_data->length,
                                          decoded_el_data->value, field_info,
                                          parent_info, ctx->opts.user_data);
        }
    } break;

    case FORMAT_JSON_STRING: {
        char* json_str_buffer = (char*) ctx->data;

        if (ctx->tlv_el_idx == 0) {
            json_str_buffer[0] = '{';
        }

        if (decoded_el_data->idx == 0 && parent_info) {
            assert((parent_info->type == FIELD_TYPE_STRUCT ||
                    parent_info->type == FIELD_TYPE_ARRAY) &&
                   parent_info->struct_type_info);

            bool is_array = parent_info->type == FIELD_TYPE_ARRAY;
            sprintf(json_str_buffer + strlen(json_str_buffer), "\"%s\":%s{",
                    parent_info->label ? parent_info->label : parent_info->name,
                    parent_info->type == FIELD_TYPE_ARRAY ? "[" : "");
        }

        // print field name
        sprintf(json_str_buffer + strlen(json_str_buffer), "\"%s\":",
                field_info->label ? field_info->label : field_info->name);

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

        case FIELD_TYPE_ARRAY: { // builtin arrays
            int n_elements = decoded_el_data->length / field_info->size;
            bool is_float_array = field_info->array_field_info.builtin_type ==
                                  S_ARRAY_BUILTIN_TYPE_FLOAT;
            bool is_string_array = field_info->array_field_info.builtin_type ==
                                   S_ARRAY_BUILTIN_TYPE_STRING;

            sprintf(json_str_buffer + strlen(json_str_buffer), "[");

            for (int i = 0; i < n_elements; i++) {
                if (i != 0)
                    sprintf(json_str_buffer + strlen(json_str_buffer), ",");

                if (is_float_array) {
                    switch (field_info->size) {
                    case 4: {
                        sprintf(json_str_buffer + strlen(json_str_buffer), "%f",
                                ((float*) decoded_el_data->value)[i]);
                    } break;
                    case 8: {
                        sprintf(json_str_buffer + strlen(json_str_buffer), "%f",
                                ((double*) decoded_el_data->value)[i]);
                    } break;
                    default:
                        ctx->err = SERIALIZER_ERROR_INVALID_TYPE;
                    }
                } else {
                    switch (field_info->size) {
                    case 1: {
                        sprintf(json_str_buffer + strlen(json_str_buffer), "%d",
                                ((int8_t*) decoded_el_data->value)[i]);
                    } break;
                    case 2: {
                        sprintf(json_str_buffer + strlen(json_str_buffer), "%d",
                                ((int16_t*) decoded_el_data->value)[i]);
                    } break;
                    case 4: {
                        sprintf(json_str_buffer + strlen(json_str_buffer), "%d",
                                ((int32_t*) decoded_el_data->value)[i]);
                    } break;
                    case 8: {
                        sprintf(json_str_buffer + strlen(json_str_buffer), "%d",
                                ((int64_t*) decoded_el_data->value)[i]);
                    } break;
                    default:
                        ctx->err = SERIALIZER_ERROR_INVALID_TYPE;
                    }
                }

                if (ctx->err != SERIALIZER_OK)
                    return;
            }

            sprintf(json_str_buffer + strlen(json_str_buffer), "]");

        } break;

        default:
            ctx->err = SERIALIZER_ERROR_INVALID_TYPE;
        }

        if (parent_info && parent_info->struct_type_info->field_count ==
                               decoded_el_data->idx + 1) {
            sprintf(json_str_buffer + strlen(json_str_buffer), "}");

            if (parent_info->type == FIELD_TYPE_ARRAY)
                sprintf(json_str_buffer + strlen(json_str_buffer), "]");
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

s_serializer_error s_deserialize(s_deserialize_options opts,
                                 const s_type_info* info, void* data,
                                 const uint8_t* buffer, size_t buffer_size) {
    if (!info || !data || !buffer || !opts.allocator) {
        return SERIALIZER_ERROR_INVALID_TYPE;
    }

    s_deserialize_context ctx = {
        .tlv_el_idx = 0,
        .info = info,
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
                        opts.allocator->deallocate(str, opts.user_data);
                    }
                }
            }
        }

        return err != SERIALIZER_OK ? err : ctx.err;
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
