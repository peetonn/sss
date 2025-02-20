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

S_SERIALIZE_BEGIN(fixed_strings_struct)
S_FIELD_STRING_FIXED(name)
S_FIELD_INT32(n_phone_numbers)
S_FIELD_ARRAY_STATIC(phone_numbers, n_phone_numbers)
S_BUILTIN_ARRAY_FIELD_SET_TYPE(phone_numbers, S_ARRAY_BUILTIN_TYPE_STRING)
S_SERIALIZE_END()

S_SERIALIZE_BEGIN(TestStruct)
S_FIELD_STRING_FIXED(str_)
S_FIELD_INT32(a_)
S_SERIALIZE_END()

S_SERIALIZE_BEGIN(TestStruct2)
S_FIELD_STRUCT(testStruct_, TestStruct)
S_SERIALIZE_END()

S_SERIALIZE_BEGIN(TestStruct3)
S_FIELD_INT32(nStaticStructs_)
S_FIELD_INT32(nDynamicStructs_)
S_FIELD_INT32(nInts_)
S_FIELD_INT32(nInts2_)
S_FIELD_STRUCT_ARRAY_STATIC(staticStructs_, nStaticStructs_, TestStruct)
S_FIELD_STRUCT_ARRAY_DYNAMIC(dynamicStructs_, nDynamicStructs_, TestStruct2)
S_FIELD_ARRAY_STATIC(ints_, nInts_)
S_FIELD_ARRAY_DYNAMIC(ints2_, nInts2_)
S_SERIALIZE_END()
// ---