/**********************************************************************
Copyright 2021 The Khronos Group
Copyright 2022 Advanced Micro Devices, Inc
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at
    http://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
********************************************************************/

#pragma once

#include "Object.h"
// helium
#include "helium/array/Array1D.h"
#include "helium/array/Array2D.h"
#include "helium/array/Array3D.h"
#include "helium/array/ObjectArray.h"

namespace anari {
namespace rpr {

// Arrays are completely handled by helium (shared, captured and managed memory, mapping, privatization)

using Array                   = helium::Array;
using Array1D                 = helium::Array1D;
using Array1DMemoryDescriptor = helium::Array1DMemoryDescriptor;
using Array2D                 = helium::Array2D;
using Array2DMemoryDescriptor = helium::Array2DMemoryDescriptor;
using Array3D                 = helium::Array3D;
using Array3DMemoryDescriptor = helium::Array3DMemoryDescriptor;
using ObjectArray             = helium::ObjectArray;

// Array parameters of objects. They return nullptr if the parameter is not an array of the expected kind.

inline Array1D *getArray1D(const helium::ParameterizedObject *object, const std::string &name)
{
  auto *array = getObjectParam<Array1D>(object, name, ANARI_ARRAY1D);
  return array && !anari::isObject(array->elementType()) ? array : nullptr;
}

inline Array2D *getArray2D(const helium::ParameterizedObject *object, const std::string &name)
{
  return getObjectParam<Array2D>(object, name, ANARI_ARRAY2D);
}

inline Array3D *getArray3D(const helium::ParameterizedObject *object, const std::string &name)
{
  return getObjectParam<Array3D>(object, name, ANARI_ARRAY3D);
}

inline ObjectArray *getObjectArray(const helium::ParameterizedObject *object, const std::string &name)
{
  auto *array = getObjectParam<Array>(object, name, ANARI_ARRAY1D);
  return array && anari::isObject(array->elementType()) ? (ObjectArray *)array : nullptr;
}

// Reads element 'i' of an array as attribute value, see the conversion rules of the ANARI specification
inline float4 readAttributeValue(const Array1D *array, size_t i)
{
  return readAsAttributeValueFlat(array->begin(), array->elementType(), i);
}

inline float4 readAttributeValue(const Array2D *array, size_t i)
{
  return readAsAttributeValueFlat(array->data(), array->elementType(), i);
}

inline float4 readAttributeValue(const Array3D *array, size_t i)
{
  return readAsAttributeValueFlat(array->data(), array->elementType(), i);
}

} // namespace rpr
} // namespace anari
