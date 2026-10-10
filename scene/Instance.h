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

#include "../rpr_common.h"
#include "Group.h"
#include "SceneObject.h"

namespace anari {
namespace rpr {

struct Instance : public SceneObject
{
 public:
  explicit Instance(RPRGlobalState *s);
  void commitParameters() override;
  void finalize() override;
  bool isValid() const override;
  box3 bounds() const override;

  void attachToScene(
      rpr_scene scene, SceneItems &items, std::vector<EnvironmentLightInstance> &environmentLights) const;

 private:
  // calls a function for every transformation of the instance
  template <typename FCN_T>
  void forEachTransform(FCN_T &&fcn) const;

  helium::IntrusivePtr<Group>        m_group;
  mat4                               m_transform{linalg::identity};
  helium::ChangeObserverPtr<Array1D> m_transform_array; // KHR_INSTANCE_TRANSFORM_ARRAY
  UniformAttributeSet                m_uniform_attributes;
  RayVisibility                      m_visibility;
};
} // namespace rpr

ANARI_TYPEFOR_SPECIALIZATION(rpr::Instance *, ANARI_INSTANCE);

} // namespace anari
