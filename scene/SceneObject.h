/**********************************************************************
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

#include "../Array.h"

namespace anari::rpr {

// Base of all objects with a spatial extent
struct SceneObject : public Object
{
  SceneObject(ANARIDataType type, RPRGlobalState *s);

  bool getProperty(
      const std::string_view &name, ANARIDataType type, void *ptr, uint64_t size, uint32_t flags) override;

  virtual box3 bounds() const;

  // every change of a scene object requires an update of the RPR scene
  void markFinalized() override;

 protected:
  void extendBounds(const box3 &bounds);
  void resetBounds();

  box3 m_bounds{emptyBox()};
};

// Inline SceneObject definitions

inline SceneObject::SceneObject(ANARIDataType type, RPRGlobalState *s) : Object(type, s) {}

inline box3 SceneObject::bounds() const
{
  return m_bounds;
}

inline void SceneObject::markFinalized()
{
  Object::markFinalized();
  markSceneChanged();
}

inline void SceneObject::extendBounds(const box3 &bounds)
{
  if (!isEmpty(bounds))
    m_bounds.extend(bounds);
}

inline void SceneObject::resetBounds()
{
  m_bounds = emptyBox();
}

inline bool SceneObject::getProperty(
    const std::string_view &name, ANARIDataType type, void *ptr, uint64_t size, uint32_t flags)
{
  if (name == "bounds" && type == ANARI_FLOAT32_BOX3)
  {
    const box3 b = bounds();
    if (isEmpty(b))
      return false;
    std::memcpy(ptr, &b, sizeof(b));
    return true;
  }

  return Object::getProperty(name, type, ptr, size, flags);
}

} // namespace anari::rpr
