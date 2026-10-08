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

#include "RPRGlobalState.h"
#include "RPRMath.h"
// helium
#include "helium/BaseObject.h"
#include "helium/utility/ChangeObserverPtr.h"
// rpr
#include "rpr_common.h"
// std
#include <string>
#include <string_view>

namespace anari {
namespace rpr {

struct Object : public helium::BaseObject
{
  Object(ANARIDataType type, RPRGlobalState *s);
  ~Object() override = default;

  bool getProperty(
      const std::string_view &name, ANARIDataType type, void *ptr, uint64_t size, uint32_t flags) override;

  void commitParameters() override;
  void finalize() override;

  bool isValid() const override;

  RPRGlobalState *deviceState() const;

 protected:
  // has to be called when the object changes something that is part of the RPR scene
  void markSceneChanged() const;
};

// Placeholder for objects of unknown subtypes. It is never valid, so it is ignored by the objects which
// use it.
struct UnknownObject : public Object
{
  UnknownObject(ANARIDataType type, std::string_view subtype, RPRGlobalState *s);
  ~UnknownObject() override = default;
  bool isValid() const override;
};

// Returns the object parameter 'name' if it is an object of the given type. Note that this can be an
// UnknownObject, so 'isValid()' has to be checked before the object is used as OBJECT_T.
template <typename OBJECT_T>
inline OBJECT_T *getObjectParam(
    const helium::ParameterizedObject *object, const std::string &name, ANARIDataType type)
{
  auto *value = object->getParamObject<helium::BaseObject>(name);
  return value && value->type() == type ? (OBJECT_T *)value : nullptr;
}

} // namespace rpr

ANARI_TYPEFOR_SPECIALIZATION(rpr::Object *, ANARI_OBJECT);

} // namespace anari
