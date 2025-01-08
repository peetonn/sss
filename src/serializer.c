/*
 * Created on Wed Jan 08 2025
 *
 * Author: Peter Gusev
 * Copyright (c) 2025 Peter Gusev. All rights reserved.
 */

#include "sss/sss.h"

#include <stdlib.h>
#include <string.h>

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

static s_serializer_error serialize_field(const s_field_info* field,
                                          const void* data,
                                          serialization_format format,
                                          uint8_t* buffer, size_t buffer_size,
                                          size_t* bytes_written) {
    const uint8_t* field_data = (const uint8_t*) data + field->offset;

    // check if field is optional and if it should be serialized
    if (is_field_present(data, field) == 0) {
        *bytes_written = 0;
        return SERIALIZER_OK;
    }

    switch (field->type) {
    case FIELD_TYPE_INT32:
    case FIELD_TYPE_UINT32:
        if (buffer_size < sizeof(uint32_t)) {
            return SERIALIZER_ERROR_BUFFER_TOO_SMALL;
        }
        memcpy(buffer, field_data, sizeof(uint32_t));
        *bytes_written = sizeof(uint32_t);
        break;

    case FIELD_TYPE_FLOAT:
        if (buffer_size < sizeof(float)) {
            return SERIALIZER_ERROR_BUFFER_TOO_SMALL;
        }
        memcpy(buffer, field_data, sizeof(float));
        *bytes_written = sizeof(float);
        break;

    case FIELD_TYPE_BOOL:
        if (buffer_size < sizeof(bool)) {
            return SERIALIZER_ERROR_BUFFER_TOO_SMALL;
        }
        memcpy(buffer, field_data, sizeof(bool));
        *bytes_written = sizeof(bool);
        break;

    case FIELD_TYPE_STRING: {
        const char* str = *(const char**) field_data;
        size_t str_len = strlen(str) + 1; // Include null terminator
        if (buffer_size < str_len) {
            return SERIALIZER_ERROR_BUFFER_TOO_SMALL;
        }
        memcpy(buffer, str, str_len);
        *bytes_written = str_len;
        break;
    }

    case FIELD_TYPE_BLOB: {
        if (buffer_size < field->size) {
            return SERIALIZER_ERROR_BUFFER_TOO_SMALL;
        }
        memcpy(buffer, field_data, field->size);
        *bytes_written = field->size;
        break;
    }

    case FIELD_TYPE_STRUCT: {
        const s_type_info* sub_info = field->struct_type_info;
        if (!sub_info) {
            return SERIALIZER_ERROR_INVALID_TYPE;
        }

        s_serializer_error err = serialize(sub_info, field_data, format, buffer,
                                           buffer_size, bytes_written);
        if (err != SERIALIZER_OK) {
            return err;
        }
        break;
    }

    default:
        return SERIALIZER_ERROR_INVALID_TYPE;
    }

    return SERIALIZER_OK;
}

s_serializer_error serialize(const s_type_info* info, const void* data,
                             serialization_format format, uint8_t* buffer,
                             size_t buffer_size, size_t* bytes_written) {
    if (!info || !data || !buffer || !bytes_written) {
        return SERIALIZER_ERROR_INVALID_TYPE;
    }

    size_t total_bytes = 0;
    uint8_t* current_buffer = buffer;
    size_t remaining_size = buffer_size;

    // Serialize each field
    for (size_t i = 0; i < info->field_count; i++) {
        size_t field_bytes = 0;
        s_serializer_error err =
            serialize_field(&info->fields[i], data, format, current_buffer,
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

static s_serializer_error
deserialize_field(const s_field_info* field, void* data,
                  serialization_format format, const uint8_t* buffer,
                  size_t buffer_size, size_t* bytes_read) {
    uint8_t* field_data = (uint8_t*) data + field->offset;

    // check if field is optional and if it should be deserialized
    if (is_field_present(data, field) == 0) {
        *bytes_read = 0;
        return SERIALIZER_OK;
    }

    switch (field->type) {
    case FIELD_TYPE_INT32:
    case FIELD_TYPE_UINT32:
        if (buffer_size < sizeof(uint32_t)) {
            return SERIALIZER_ERROR_BUFFER_TOO_SMALL;
        }
        memcpy(field_data, buffer, sizeof(uint32_t));
        *bytes_read = sizeof(uint32_t);
        break;

    case FIELD_TYPE_FLOAT:
        if (buffer_size < sizeof(float)) {
            return SERIALIZER_ERROR_BUFFER_TOO_SMALL;
        }
        memcpy(field_data, buffer, sizeof(float));
        *bytes_read = sizeof(float);
        break;

    case FIELD_TYPE_BOOL:
        if (buffer_size < sizeof(bool)) {
            return SERIALIZER_ERROR_BUFFER_TOO_SMALL;
        }
        memcpy(field_data, buffer, sizeof(bool));
        *bytes_read = sizeof(bool);
        break;

    case FIELD_TYPE_STRING: {
        // Find string length (including null terminator)
        size_t str_len = 0;
        while (str_len < buffer_size && buffer[str_len] != '\0') {
            str_len++;
        }
        if (str_len >= buffer_size) { // No null terminator found
            return SERIALIZER_ERROR_BUFFER_TOO_SMALL;
        }
        str_len++; // Include null terminator

        // Allocate and copy string
        char* str = malloc(str_len);
        if (!str) {
            return SERIALIZER_ERROR_BUFFER_TOO_SMALL; // Could add a new error
                                                      // type
        }
        memcpy(str, buffer, str_len);
        *(char**) field_data = str; // Store pointer to new string
        *bytes_read = str_len;
        break;
    }

    case FIELD_TYPE_BLOB: {
        if (buffer_size < field->size) {
            return SERIALIZER_ERROR_BUFFER_TOO_SMALL;
        }
        memcpy(field_data, buffer, field->size);
        *bytes_read = field->size;
        break;
    }

    case FIELD_TYPE_STRUCT: {
        const s_type_info* sub_info = field->struct_type_info;
        if (!sub_info) {
            return SERIALIZER_ERROR_INVALID_TYPE;
        }

        s_serializer_error err = deserialize(sub_info, field_data, format,
                                             buffer, buffer_size, NULL);
        if (err != SERIALIZER_OK) {
            return err;
        }

        *bytes_read = buffer_size;
        break;
    }

    default:
        return SERIALIZER_ERROR_INVALID_TYPE;
    }

    return SERIALIZER_OK;
}

s_serializer_error deserialize(const s_type_info* info, void* data,
                               serialization_format format,
                               const uint8_t* buffer, size_t buffer_size,
                               s_allocator* allocator) {
    if (!info || !data || !buffer) {
        return SERIALIZER_ERROR_INVALID_TYPE;
    }

    const uint8_t* current_buffer = buffer;
    size_t remaining_size = buffer_size;

    // Deserialize each field
    for (size_t i = 0; i < info->field_count; i++) {
        size_t bytes_read = 0;
        s_serializer_error err =
            deserialize_field(&info->fields[i], data, format, current_buffer,
                              remaining_size, &bytes_read);

        if (err != SERIALIZER_OK) {
            return err;
        }

        current_buffer += bytes_read;
        remaining_size -= bytes_read;
    }

    return SERIALIZER_OK;
}