/*
 * Created on Wed Jan 08 2025
 *
 * Author: Peter Gusev
 * Copyright (c) 2025 Peter Gusev. All rights reserved.
 */

#include "sss/serializer.h"

#include "sss/log.h"
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
#define MAX_TLV_ELEMS (1024)

struct tlv_el_context {
    int prev_level;
    int prev_idx;
    const s_type_info* field_type_info;
    const s_field_info* field_info;
    const s_type_info* parent_type_info;
    const s_field_info* parent_info;
    int parent_field_idx;
};

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

void s_deserialize_field_c_struct(
    s_deserialize_context* ctx, int field_idx, const void* type_data,
    const s_type_info* type_info, const s_field_info* parent_info,
    const s_tlv_decoded_element_data* decoded_el_data);
void s_deserialize_field_json_string(
    s_deserialize_context* ctx, int field_idx, const void* type_data,
    const s_type_info* type_info, const s_field_info* parent_info,
    const s_tlv_decoded_element_data* decoded_el_data);
void s_deserialize_field(s_deserialize_context* ctx, int field_idx,
                         const void* type_data, const s_type_info* type_info,
                         const s_field_info* parent_info,
                         const s_tlv_decoded_element_data* decoded_el_data);

void tlv_decode_deserializer_cb(
    const s_tlv_decoded_element_data* decoded_el_data, void* user_data) {
    s_deserialize_context* ctx = (s_deserialize_context*) user_data;

    // end of decoding call
    if (!decoded_el_data) {
        switch (ctx->opts.format) {
        case FORMAT_JSON_STRING:
            s_deserialize_field_json_string(ctx, 0, ctx->data, ctx->info, NULL,
                                            NULL);
            break;
        case FORMAT_CUSTOM: {
            if (ctx->opts.custom_deserializer)
                ctx->opts.custom_deserializer(-1, 0, 0, NULL, NULL, NULL, NULL,
                                              ctx->opts.user_data);
        } break;
        default:
            break;
        }

        return;
    }

    int lvl = 0;
    struct type_info_stack_el {
        const s_field_info* parent_info;
        const s_type_info* type_info;
        int last_field_idx;
        void* data;
        void* array_data;
        uint32_t array_size;
        uint32_t pending_array_el_count;
    } type_info_stack[MAX_NESTED_LEVELS] = {{NULL, ctx->info, 0, ctx->data}};

    // find field_info for current element
    const s_type_info* type_info = type_info_stack[0].type_info;
    void* data = type_info_stack[0].data;
    const s_type_info* parent_type_info = NULL;
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
                type_info_stack[lvl].array_data = NULL;
                type_info_stack[lvl].array_size = 0;
                type_info_stack[lvl].pending_array_el_count = 0;

                parent_type_info = type_info;
                parent_info = type_info_stack[lvl].parent_info;
                type_info = type_info_stack[lvl].type_info;

                // if we got nested element callback -- quit here
                bool decoded_el_parent_is_array =
                    type_info_stack[decoded_el_data->level].parent_info
                        ? type_info_stack[decoded_el_data->level]
                                  .parent_info->type == FIELD_TYPE_ARRAY
                        : false;

                if (decoded_el_data->type == TLV_TAG_NESTED &&
                    lvl == decoded_el_data->level + 1 &&
                    (field_idx == decoded_el_data->idx ||
                     decoded_el_parent_is_array)) {
                    type_info = parent_type_info;
                    field_info = parent_info;
                    lvl -= 1;
                } else {
                    field_idx = 0;
                }

                tlv_idx++;
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
                    size_t array_size_field_offset =
                        type_info->fields[field_idx]
                            .array_field_info.size_field_offset;
                    const uint8_t* size_type_data = NULL;
                    // find array size in previously decoded element
                    for (int i = ctx->tlv_el_idx; i >= 0, !size_type_data;
                         i--) {
                        if (ctx->decoded_els[i].type_info == type_info &&
                            type_info->fields[ctx->decoded_els[i].field_idx]
                                    .offset == array_size_field_offset) {
                            size_type_data = ctx->decoded_els[i].el.value;
                        }
                    }

                    if (!size_type_data) {
                        LOG_DEBUG("ERROR (decode cb): no size field data found "
                                  "in array field");
                        ctx->err = SERIALIZER_ERROR_INVALID_TYPE;
                        return;
                    }

                    switch (type_info->fields[field_idx]
                                .array_field_info.size_field_size) {
                    case 1: {
                        array_size = (uint32_t) *(uint8_t*) size_type_data;
                    } break;
                    case 2: {
                        array_size = (uint32_t) *(uint16_t*) size_type_data;
                    } break;
                    case 4:
                    case 8: {
                        array_size = *(uint32_t*) size_type_data;
                    } break;
                    default: {
                        LOG_DEBUG("ERROR (decode cb): invalid size field size");
                        ctx->err = SERIALIZER_ERROR_INVALID_TYPE;
                    }
                        return;
                    }

                    if (is_dynamic) {
                        // special case for c structs -- allocate dynamic array
                        // here
                        if (ctx->opts.format == FORMAT_C_STRUCT) {
                            void** array_data_ptr =
                                (void**) ((uint8_t*) data +
                                          type_info->fields[field_idx].offset);

                            if (!*array_data_ptr && array_size) {
                                *array_data_ptr = ctx->opts.allocator->allocate(
                                    array_size *
                                        type_info->fields[field_idx]
                                            .struct_type_info->type_size,
                                    ctx->opts.user_data);
                            }

                            if (!*array_data_ptr) {
                                LOG_DEBUG(
                                    "ERROR (decode cb): failed to allocate "
                                    "memory for dynamic array");
                                ctx->err = SERIALIZER_ERROR_ALLOCATOR_FAILED;
                                return;
                            }

                            data = *array_data_ptr;
                        }
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
                    type_info_stack[lvl].array_data = data;
                    type_info_stack[lvl].array_size = array_size;
                    type_info_stack[lvl].pending_array_el_count = array_size;

                    parent_type_info = type_info;
                    parent_info = type_info_stack[lvl].parent_info;
                    type_info = type_info_stack[lvl].type_info;

                    if (decoded_el_data->type == TLV_TAG_NESTED_LIST &&
                        lvl == decoded_el_data->level + 1 &&
                        field_idx == decoded_el_data->idx) {
                        type_info = parent_type_info;
                        field_info = parent_info;
                        lvl -= 1;
                    } else {
                        field_idx = 0;
                    }

                    tlv_idx++;
                    continue;
                    // break;
                }
            } // fallthrough for non-struct arrays
            default: {
                if (tlv_idx == ctx->tlv_el_idx) {
                    field_info = &type_info->fields[field_idx];

                    if (!parent_info || parent_info->type != FIELD_TYPE_ARRAY) {
                        ctx->decoded_els[ctx->tlv_el_idx].el = *decoded_el_data;
                        ctx->decoded_els[ctx->tlv_el_idx].type_info = type_info;
                        ctx->decoded_els[ctx->tlv_el_idx].field_idx = field_idx;
                    }
                } else {
                    tlv_idx++;
                    field_idx++;
                }
            } break;
            }
        } else {
            // if we're iterating array, check if we're not done yet
            if (type_info_stack[lvl].pending_array_el_count > 0) {
                type_info_stack[lvl].pending_array_el_count -= 1;
                int idx = type_info_stack[lvl].array_size -
                          type_info_stack[lvl].pending_array_el_count;
                // shift data pointer to next element
                data = type_info_stack[lvl].array_data +
                       type_info_stack[lvl].type_info->type_size * idx;
                field_idx = 0;

                if (type_info_stack[lvl].pending_array_el_count)
                    continue;
            }

            lvl -= 1;

            if (lvl < 0) {
                LOG_DEBUG("ERROR (decode cb): invalid nested level");
                ctx->err = SERIALIZER_ERROR_INVALID_TYPE;
                return;
            }

            parent_type_info =
                lvl > 0 ? type_info_stack[lvl - 1].type_info : NULL;
            parent_info = type_info_stack[lvl].parent_info;
            type_info = type_info_stack[lvl].type_info;
            field_idx = type_info_stack[lvl].last_field_idx + 1;
            data = type_info_stack[lvl].data;
        }
    }

    assert(decoded_el_data->level == lvl);
    LOG_DEBUG("%d MATCH %s::%s (PARENT %s::%s)", ctx->tlv_el_idx,
              type_info->type_name, field_info->name,
              parent_type_info ? parent_type_info->type_name : "none",
              parent_info ? parent_info->name : "none");

    s_deserialize_field(ctx, field_idx, data, type_info, parent_info,
                        decoded_el_data);

    ctx->tlv_el_idx++;
    ctx->prev_level = decoded_el_data->level;
}

s_serializer_error s_deserialize(s_deserialize_options opts,
                                 const s_type_info* info, void* data,
                                 const uint8_t* buffer, size_t buffer_size) {
    if (!info || !data || !buffer || !opts.allocator) {
        LOG_DEBUG("ERROR (deserialize): invalid arguments");
        return SERIALIZER_ERROR_INVALID_TYPE;
    }

    s_deserialize_context ctx = {
        .tlv_el_idx = 0,
        .prev_level = -1,
        .info = info,
        .opts = opts,
        .data = data,
        .n_allocations = 0,
        .err = SERIALIZER_OK,
        .decoded_els = {},
        .json_context = {},
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
    // if (opts.format == FORMAT_JSON_STRING) {
    //     char* json_str_buffer = (char*) ctx.data;
    //     json_str_buffer[strlen(json_str_buffer) - 1] = '}';
    // }

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

// helpers
void s_deserialize_field(s_deserialize_context* ctx, int field_idx,
                         const void* type_data, const s_type_info* type_info,
                         const s_field_info* parent_info,
                         const s_tlv_decoded_element_data* decoded_el_data) {
    const s_field_info* field_info = &type_info->fields[field_idx];

    switch (ctx->opts.format) {

    case FORMAT_C_STRUCT: {
        s_deserialize_field_c_struct(ctx, field_idx, type_data, type_info,
                                     parent_info, decoded_el_data);
    } break;

    case FORMAT_JSON_STRING: {
        s_deserialize_field_json_string(ctx, field_idx, type_data, type_info,
                                        parent_info, decoded_el_data);
    } break;

    case FORMAT_CUSTOM: {
        if (ctx->opts.custom_deserializer) {
            ctx->opts.custom_deserializer(
                decoded_el_data->idx, decoded_el_data->level,
                decoded_el_data->length, decoded_el_data->value, field_info,
                type_info, parent_info, ctx->opts.user_data);
        }
    } break;

    default: {
        LOG_DEBUG("ERROR (deserialize): invalid format");
        ctx->err = SERIALIZER_ERROR_INVALID_TYPE;
    } break;
    }
}

void s_deserialize_field_c_struct(
    s_deserialize_context* ctx, int field_idx, const void* type_data,
    const s_type_info* type_info, const s_field_info* parent_info,
    const s_tlv_decoded_element_data* decoded_el_data) {
    void* dest_ptr = NULL;
    const s_field_info* field_info = &type_info->fields[field_idx];

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
        dest_ptr = (uint8_t*) type_data + field_info->offset;
        memcpy(dest_ptr, decoded_el_data->value, decoded_el_data->length);
    } break;
    case FIELD_TYPE_STRING: {
        if (field_info->opts & S_FIELD_OPT_STRING_FIXED) {
            dest_ptr = (uint8_t*) type_data + field_info->offset;
            memcpy(dest_ptr, decoded_el_data->value, decoded_el_data->length);
        } else {
            // need allocation
            if (decoded_el_data->length) {
                dest_ptr = ctx->opts.allocator->allocate(
                    decoded_el_data->length, ctx->opts.user_data);

                if (!dest_ptr) {
                    if (ctx->err == SERIALIZER_OK)
                        ctx->err = SERIALIZER_ERROR_ALLOCATOR_FAILED;

                    LOG_DEBUG("ERROR (deserialize): failed to allocate memory "
                              "for string field");
                    return;
                }

                ctx->n_allocations++;
                memset(dest_ptr, 0, decoded_el_data->length);
                memcpy(dest_ptr, decoded_el_data->value,
                       decoded_el_data->length);
            }

            *(char**) ((uint8_t*) type_data + field_info->offset) = dest_ptr;
        }
    } break;
    case FIELD_TYPE_ARRAY: {
        bool is_struct_array = field_info->struct_type_info;
        bool is_dynamic_array = field_info->opts & S_FIELD_OPT_ARRAY_DYNAMIC;

        // struct arrays are handled in the caller of this function
        if (is_struct_array) {
            break;
        }

        if (is_dynamic_array) {
            dest_ptr = ctx->opts.allocator->allocate(decoded_el_data->length,
                                                     ctx->opts.user_data);

            if (!dest_ptr) {
                if (ctx->err == SERIALIZER_OK)
                    ctx->err = SERIALIZER_ERROR_ALLOCATOR_FAILED;

                LOG_DEBUG("ERROR (deserialize): failed to allocate memory for "
                          "dynamic array field");
                return;
            }

            ctx->n_allocations++;
            memcpy(dest_ptr, decoded_el_data->value, decoded_el_data->length);

            *(void**) ((uint8_t*) type_data + field_info->offset) = dest_ptr;
        } else {
            if (field_info->array_field_info.builtin_type &
                S_ARRAY_BUILTIN_TYPE_STRING) {
                dest_ptr = (uint8_t*) type_data + field_info->offset;
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
                dest_ptr = (uint8_t*) type_data + field_info->offset;
                memcpy(dest_ptr, decoded_el_data->value,
                       decoded_el_data->length);
            }
        }
    } break;
    case FIELD_TYPE_STRUCT: { // do nothing
    } break;

    default: {
        LOG_DEBUG("ERROR (deserialize): invalid field type");
        ctx->err = SERIALIZER_ERROR_INVALID_TYPE;
    } break;
    }
}

#define JSON_OPEN_BRACE_OBJECT ("{")
#define JSON_OPEN_BRACE_ARRAY ("[")
// #define JSON_OPEN_BRACE_OBJECT_ARRAY ("[{")
#define JSON_CLOSE_BRACE_OBJECT ("}")
#define JSON_CLOSE_BRACE_ARRAY ("]")
// #define JSON_CLOSE_BRACE_OBJECT_ARRAY ("}]")

#define JSON_APPEND(buf, str) sprintf(buf + strlen(buf), "%s", str)
#define JSON_APPENDF(buf, fmt, ...) sprintf(buf + strlen(buf), fmt, __VA_ARGS__)
#define JSON_PUSH_BRACE(ctx, lvl, brace)                                      \
    ctx->json_context                                                         \
        .closing_braces[lvl][strlen(ctx->json_context.closing_braces[lvl])] = \
        brace[0]

void s_deserialize_field_json_string(
    s_deserialize_context* ctx, int field_idx, const void* type_data,
    const s_type_info* type_info, const s_field_info* parent_info,
    const s_tlv_decoded_element_data* decoded_el_data) {

    // check for endo of decoding here
    if (!decoded_el_data) {
        // close all pending braces
        for (int i = ctx->prev_level; i >= 0; i--) {
            for (int j = strlen(ctx->json_context.closing_braces[i]) - 1;
                 j >= 0; j--) {
                JSON_APPENDF(ctx->data, "%c",
                             ctx->json_context.closing_braces[i][j]);
            }
        }

        return;
    }

    char* json_str_buffer = (char*) ctx->data;
    const s_field_info* field_info = &type_info->fields[field_idx];
    const char* field_label =
        field_info->label ? field_info->label : field_info->name;
    bool root_start = ctx->prev_level == -1;
    bool level_dropped = ctx->prev_level > decoded_el_data->level;
    bool new_nested_started =
        field_idx == 0 && ctx->prev_level == decoded_el_data->level;
    bool is_struct_array =
        field_info->type == FIELD_TYPE_ARRAY && field_info->struct_type_info;

    // check closing braces
    // 1. when level went down
    // 2. when level same, index 0
    if (level_dropped) {
        for (int i = ctx->prev_level; i > decoded_el_data->level; i--) {
            for (int j = strlen(ctx->json_context.closing_braces[i]) - 1;
                 j >= 0; j--) {
                JSON_APPENDF(json_str_buffer, "%c",
                             ctx->json_context.closing_braces[i][j]);
                ctx->json_context.closing_braces[i][j] = 0;
            }
        }
    }

    if (new_nested_started) {
        JSON_APPENDF(json_str_buffer, "%s", JSON_CLOSE_BRACE_OBJECT);
    }

    // add coma if not first element
    if (decoded_el_data->idx != 0) {
        JSON_APPEND(json_str_buffer, ",");
    }

    if (new_nested_started || root_start) {
        JSON_APPEND(json_str_buffer, JSON_OPEN_BRACE_OBJECT);
        if (root_start)
            JSON_PUSH_BRACE(ctx, decoded_el_data->level,
                            JSON_CLOSE_BRACE_OBJECT);
    }

    JSON_APPENDF(json_str_buffer, "\"%s\":", field_label);

    if (is_struct_array) {
        JSON_APPEND(json_str_buffer, JSON_OPEN_BRACE_ARRAY);
        JSON_PUSH_BRACE(ctx, decoded_el_data->level + 1,
                        JSON_CLOSE_BRACE_ARRAY);
        JSON_APPEND(json_str_buffer, JSON_OPEN_BRACE_OBJECT);
        JSON_PUSH_BRACE(ctx, decoded_el_data->level + 1,
                        JSON_CLOSE_BRACE_OBJECT);
    }

    if (field_info->type == FIELD_TYPE_STRUCT) {
        JSON_APPEND(json_str_buffer, JSON_OPEN_BRACE_OBJECT);
        JSON_PUSH_BRACE(ctx, decoded_el_data->level + 1,
                        JSON_CLOSE_BRACE_OBJECT);
    }

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
        JSON_APPENDF(json_str_buffer, "%d", *(int*) decoded_el_data->value);
    } break;

    case FIELD_TYPE_FLOAT:
    case FIELD_TYPE_DOUBLE: {
        JSON_APPENDF(json_str_buffer, "%f", *(float*) decoded_el_data->value);
    } break;

    case FIELD_TYPE_BOOL: {
        JSON_APPEND(json_str_buffer,
                    *(bool*) decoded_el_data->value ? "true" : "false");
    } break;

    case FIELD_TYPE_BLOB: {
        JSON_APPEND(json_str_buffer, JSON_OPEN_BRACE_ARRAY);

        for (size_t i = 0; i < decoded_el_data->length; i++) {
            if (i != 0)
                JSON_APPEND(json_str_buffer, ",");

            JSON_APPENDF(json_str_buffer, "%d",
                         ((uint8_t*) decoded_el_data->value)[i]);
        }

        JSON_APPEND(json_str_buffer, JSON_CLOSE_BRACE_ARRAY);
    } break;

    case FIELD_TYPE_STRING: {
        JSON_APPENDF(json_str_buffer, "\"%s\"", (char*) decoded_el_data->value);
    } break;

    case FIELD_TYPE_ARRAY: {
        bool is_struct_array = field_info->struct_type_info;
        bool is_dynamic_array = field_info->opts & S_FIELD_OPT_ARRAY_DYNAMIC;

        if (is_struct_array)
            break;

        // builtin arrays
        int n_elements = decoded_el_data->length / field_info->size;
        bool is_float_array = field_info->array_field_info.builtin_type ==
                              S_ARRAY_BUILTIN_TYPE_FLOAT;
        bool is_string_array = field_info->array_field_info.builtin_type ==
                               S_ARRAY_BUILTIN_TYPE_STRING;

        JSON_APPEND(json_str_buffer, JSON_OPEN_BRACE_ARRAY);

        for (int i = 0; i < n_elements; i++) {
            if (i != 0)
                JSON_APPEND(json_str_buffer, ",");

            switch (field_info->array_field_info.builtin_type) {
            case S_ARRAY_BUILTIN_TYPE_FLOAT: {
                switch (field_info->size) {
                case 4: {
                    JSON_APPENDF(json_str_buffer, "%f",
                                 ((float*) decoded_el_data->value)[i]);
                } break;
                case 8: {
                    JSON_APPENDF(json_str_buffer, "%f",
                                 ((double*) decoded_el_data->value)[i]);
                } break;
                default: {
                    LOG_DEBUG("ERROR (json deserialize): invalid float array "
                              "el size %d",
                              field_info->size);
                    ctx->err = SERIALIZER_ERROR_INVALID_TYPE;
                } break;
                }
            } break;
            case S_ARRAY_BUILTIN_TYPE_STRING: {
                JSON_APPENDF(json_str_buffer, "\"%s\"",
                             ((char**) decoded_el_data->value)[i]);
            } break;
            default: {
                switch (field_info->size) {
                case 1: {
                    JSON_APPENDF(json_str_buffer, "%d",
                                 ((int8_t*) decoded_el_data->value)[i]);
                } break;
                case 2: {
                    JSON_APPENDF(json_str_buffer, "%d",
                                 ((int16_t*) decoded_el_data->value)[i]);
                } break;
                case 4: {
                    JSON_APPENDF(json_str_buffer, "%d",
                                 ((int32_t*) decoded_el_data->value)[i]);
                } break;
                case 8: {
                    JSON_APPENDF(json_str_buffer, "%d",
                                 ((int64_t*) decoded_el_data->value)[i]);
                } break;
                default: {
                    LOG_DEBUG("ERROR (json deserialize): invalid builtin array "
                              "el size %d",
                              field_info->size);
                    ctx->err = SERIALIZER_ERROR_INVALID_TYPE;
                } break;
                }
            }
            }

            if (ctx->err != SERIALIZER_OK)
                return;
        }

        JSON_APPEND(json_str_buffer, JSON_CLOSE_BRACE_ARRAY);

    } break;

    case FIELD_TYPE_STRUCT:
        break; // do nothing here
    default: {
        LOG_DEBUG("ERROR (json deserialize): invalid field type %d",
                  field_info->type);
        ctx->err = SERIALIZER_ERROR_INVALID_TYPE;
    } break;
    }

    ctx->prev_level = decoded_el_data->level;
}