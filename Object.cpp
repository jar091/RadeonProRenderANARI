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

#include "Object.h"
// std
#include <string>

namespace anari {
namespace rpr {

// Object definitions /////////////////////////////////////////////////////////

Object::Object(ANARIDataType type, RPRGlobalState *s) : helium::BaseObject(type, s)
{
  // helium only commits objects with changed parameters, but an object without any parameter (for
  // example a material with default values) still has to be finalized once
  markParameterChanged();
}

void Object::commitParameters()
{
  // no-op
}

void Object::finalize()
{
  // no-op
}

bool Object::getProperty(
    const std::string_view &name, ANARIDataType type, void *ptr, uint64_t size, uint32_t flags)
{
  if (name == "valid" && type == ANARI_BOOL)
  {
    helium::writeToVoidP(ptr, isValid());
    return true;
  }

  return false;
}

bool Object::isValid() const
{
  return true;
}

RPRGlobalState *Object::deviceState() const
{
  return (RPRGlobalState *)helium::BaseObject::m_state;
}

void Object::markSceneChanged() const
{
  deviceState()->objectUpdates.lastSceneChange = helium::newTimeStamp();
}

// UnknownObject definitions //////////////////////////////////////////////////

UnknownObject::UnknownObject(ANARIDataType type, std::string_view subtype, RPRGlobalState *s)
    : Object(type, s)
{
  reportMessage(ANARI_SEVERITY_WARNING, "unsupported %s subtype '%s', the object is ignored",
      anari::toString(type), std::string(subtype).c_str());
}

bool UnknownObject::isValid() const
{
  return false;
}

} // namespace rpr

ANARI_TYPEFOR_DEFINITION(rpr::Object *);

} // namespace anari
