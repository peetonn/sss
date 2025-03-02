#include "sss/sss.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum response_status {
    RESPONSE_OK,
    RESPONSE_ERROR,
};

enum message_type {
    RESPONSE,
    CANDIDATE,
    DESCRIPTION,
};

enum description_format {
    FORMAT_A,
    FORMAT_B,
};

typedef struct {
    enum response_status status;
    char* data;
} response;

typedef struct {
    char* identifier;
    int index;
    char* value;
} candidate;

typedef struct {
    enum description_format format;
    char* content;
} description;

typedef struct {
    enum message_type type;
    union {
        response r;
        candidate c;
        description d;
    } data;
} protocol;

S_SERIALIZE_BEGIN(response)
S_FIELD_ENUM(status)
S_FIELD_STRING(data)
S_SERIALIZE_END()

S_SERIALIZE_BEGIN(candidate)
S_FIELD_STRING(identifier)
S_FIELD_INT32(index)
S_FIELD_STRING(value)
S_SERIALIZE_END()

S_SERIALIZE_BEGIN(description)
S_FIELD_ENUM(format)
S_FIELD_STRING(content)
S_SERIALIZE_END()

S_SERIALIZE_BEGIN(protocol)
S_FIELD_ENUM(type)

S_UNION_BEGIN_TAG(data, type)
S_FIELD_STRUCT(data.r, response, "response")
S_FIELD_TAGGED_INT(data.r, RESPONSE)

S_FIELD_STRUCT(data.c, candidate, "candidate")
S_FIELD_TAGGED_INT(data.c, CANDIDATE)

S_FIELD_STRUCT(data.d, description, "description")
S_FIELD_TAGGED_INT(data.d, DESCRIPTION)
S_UNION_END()

S_SERIALIZE_END()

void print_protocol_message(const protocol* p) {
    switch (p->type) {
    case RESPONSE:
        printf("Response {\n");
        printf("    status: %s\n",
               p->data.r.status == RESPONSE_OK ? "OK" : "ERROR");
        printf("    data: %s\n", p->data.r.data);
        printf("}\n");
        break;
    case CANDIDATE:
        printf("Candidate {\n");
        printf("    identifier: %s\n", p->data.c.identifier);
        printf("    index: %d\n", p->data.c.index);
        printf("    value: %s\n", p->data.c.value);
        printf("}\n");
        break;
    case DESCRIPTION:
        printf("Description {\n");
        printf("    format: %s\n", p->data.d.format == FORMAT_A ? "A" : "B");
        printf("    content: %s\n", p->data.d.content);
        printf("}\n");
        break;
    }
}

void* my_allocate(size_t size, void*) {
    return malloc(size);
}

void my_deallocate(void* data, void*) {
    free(data);
}

static s_allocator my_allocator = {.allocate = my_allocate,
                                   .deallocate = my_deallocate};
int main() {
    protocol p = {
        .type = CANDIDATE,
        .data = {.c = {.identifier = "test",
                       .index = 42, // NOLINT
                       .value = "value"}},
    };

    printf("Original:\n");
    print_protocol_message(&p);

    uint8_t buffer[4096]; // NOLINT
    size_t bytes_written = 0;
    s_serialize_options opts = {};
    s_serializer_error err =
        s_serialize(opts, S_GET_STRUCT_TYPE_INFO(protocol), &p, buffer,
                    sizeof(buffer), &bytes_written);

    if (err != SERIALIZER_OK) {
        printf("Serialization failed with error: %d\n", err);
        return 1;
    }
    printf("\nSerialized size: %zu bytes\n", bytes_written);

    // Deserialize into a new struct
    s_deserialize_options d_opts = {
        .format = FORMAT_C_STRUCT,
        .allocator = &my_allocator,
    };
    protocol deserialized = {};
    err = s_deserialize(d_opts, S_GET_STRUCT_TYPE_INFO(protocol), &deserialized,
                        buffer, bytes_written);

    if (err != SERIALIZER_OK) {
        printf("Deserialization failed with error: %d\n", err);
        return 1;
    }

    printf("\nDeserialized:\n");
    print_protocol_message(&deserialized);

    return 0;
}