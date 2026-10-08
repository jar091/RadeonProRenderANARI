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

#include "../../rpr_common.h"
#include "../SceneObject.h"
#include "../geometry/Surface.h"

namespace anari {
namespace rpr {

struct Light : public SceneObject
{
 public:
  explicit Light(RPRGlobalState *s);
  static Light *createInstance(std::string_view type, RPRGlobalState *s);

  void commitParameters() override;

  // creates the RPR objects of the light for an instance and attaches them to the scene
  virtual void attachToScene(rpr_scene scene, const mat4 &transform, SceneItems &items) = 0;

  // The environment of a RPR scene is a single light which also contains the ambient light of the
  // renderer, it is managed by the world.
  virtual bool isEnvironmentLight() const;

  bool visible() const;

 protected:
  float3 m_color{1.f};
  bool   m_visible{true};
};

} // namespace rpr

ANARI_TYPEFOR_SPECIALIZATION(rpr::Light *, ANARI_LIGHT)

} // namespace anari
