/*
 * Created on Thu Jan 09 2025
 *
 * Author: Peter Gusev
 * Copyright (c) 2025 Peter Gusev. All rights reserved.
 */

#include "common.h"

// --- SSS type info for structs
S_SERIALIZE_BEGIN(simple_struct)
S_FIELD_INT32(id, "Id")
S_FIELD_FLOAT(value)
S_FIELD_BOOL(active)
S_FIELD_STRING_CONST(name)
S_FIELD_STRING(passport_number, "PassportNumber")
S_FIELD_BLOB(blob, "Data")
S_SERIALIZE_END()

S_SERIALIZE_BEGIN(nested_struct)
S_FIELD_ENUM(id)
S_FIELD_STRUCT(sub, simple_struct)
S_FIELD_STRING_CONST(name)
S_SERIALIZE_END()

S_SERIALIZE_BEGIN(nested_union_struct)
S_FIELD_ENUM(id)
S_UNION_BEGIN_TAG(data, id)
S_FIELD_STRUCT(data.sub, simple_struct, "Sub")
S_FIELD_TAGGED_INT(data.sub, ENUM_VALUE_1)
S_FIELD_INT32(data.value)
S_FIELD_TAGGED_INT(data.value, ENUM_VALUE_2)
S_FIELD_BLOB(data.name)
S_FIELD_TAGGED_INT(data.name, ENUM_VALUE_3)
S_UNION_END()
S_SERIALIZE_END()

S_SERIALIZE_BEGIN(builtin_arrays_struct)
S_FIELD_INT32(n_static_ints)
S_FIELD_ARRAY_STATIC(static_ints, n_static_ints, "StaticInts")
S_FIELD_INT32(n_dynamic_ints)
S_FIELD_ARRAY_DYNAMIC(dynamic_ints, n_dynamic_ints, "DynamicInts")
S_SERIALIZE_END()

S_SERIALIZE_BEGIN(struct_arrays_struct)
S_FIELD_INT32(n_static_structs)
S_FIELD_STRUCT_ARRAY_STATIC(static_structs, n_static_structs, simple_struct)
S_FIELD_INT32(n_dynamic_structs)
S_FIELD_STRUCT_ARRAY_DYNAMIC(dynamic_structs, n_dynamic_structs, simple_struct,
                             "DynamicStructs")
S_SERIALIZE_END()
// ---