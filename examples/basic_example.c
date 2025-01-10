#include "sss/sss.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int32_t id;
    float value;
} SubStruct;

// Define our test structure
typedef struct {
    int32_t id;
    float value;
    bool active;
    const char* name;
    uint8_t blob[32]; // NOLINT
    SubStruct sub;
} TestStruct;

S_SERIALIZE_BEGIN(SubStruct)
S_FIELD_INT32(id)
S_FIELD_FLOAT(value)
S_SERIALIZE_END()

S_SERIALIZE_BEGIN(TestStruct)
S_FIELD_INT32(id, "Id")
S_FIELD_FLOAT(value)
S_FIELD_BOOL(active)
S_FIELD_STRING_CONST(name)
S_FIELD_BLOB(blob, "data")
S_FIELD_STRUCT(sub, SubStruct)
S_SERIALIZE_END()

void print_test_struct(const TestStruct* ts) {
    printf("TestStruct {\n");
    printf("    id: %d\n", ts->id);
    printf("    value: %f\n", ts->value);
    printf("    active: %s\n", ts->active ? "true" : "false");
    printf("    name: %s\n", ts->name);
    printf("    blob: [");
    for (size_t i = 0; i < sizeof(ts->blob); i++) {
        printf("%02X", ts->blob[i]);
    }
    printf("]\n");
    printf("    sub: {\n");
    printf("        id: %d\n", ts->sub.id);
    printf("        value: %f\n", ts->sub.value);
    printf("    }\n");
    printf("}\n");
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
    // Create test data
    TestStruct original = {.id = 42,       // NOLINT
                           .value = 3.14f, // NOLINT
                           .active = true,
                           .name = "Hello, World!"};

    original.blob[0] = 0x01; // NOLINT
    original.blob[1] = 0x02; // NOLINT
    original.blob[2] = 0x03; // NOLINT
    original.blob[3] = 0x04; // NOLINT

    original.sub = (SubStruct) {.id = 123, .value = 2.718f}; // NOLINT

    printf("Original:\n");
    print_test_struct(&original);

    uint8_t buffer[256]; // NOLINT
    size_t bytes_written = 0;
    s_serialize_options opts = {};
    s_serializer_error err =
        s_serialize(opts, S_GET_STRUCT_TYPE_INFO(TestStruct), &original, buffer,
                    sizeof(buffer), &bytes_written);

    if (err != SERIALIZER_OK) {
        printf("Serialization failed with error: %d\n", err);
        return 1;
    }
    printf("\nSerialized size: %zu bytes\n", bytes_written);

    s_deserialize_options d_opts = {.allocator = &my_allocator,
                                    .format = FORMAT_C_STRUCT};

    TestStruct deserialized = {0};
    err = s_deserialize(d_opts, S_GET_STRUCT_TYPE_INFO(TestStruct),
                        &deserialized, buffer, bytes_written);

    if (err != SERIALIZER_OK) {
        printf("Deserialization failed with error: %d\n", err);
        return 1;
    }

    printf("\nDeserialized:\n");
    print_test_struct(&deserialized);

    free((void*) deserialized.name);

    return 0;
}