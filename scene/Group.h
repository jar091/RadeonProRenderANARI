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
#include "RayVisibility.h"
#include "SceneObject.h"
#include "geometry/Surface.h"

namespace anari {
namespace rpr {

struct HdriLight;

// instance of an environment light which was found while the scene was built
struct EnvironmentLightInstance
{
  HdriLight *light{nullptr};
  mat4       transform{linalg::identity};
};

// Surfaces, volumes and lights of a group or of a world
struct GroupContent
{
  explicit GroupContent(Object *owner);

  // reads the parameters 'surface', 'volume' and 'light' of the owner
  void commit();

  box3 bounds() const;

  // 'visibility' is applied to the surfaces. Lights and volumes are not affected by it.
  void attachToScene(rpr_scene scene, const mat4 &transform, const UniformAttributeSet &instanceAttributes,
      const RayVisibility &visibility, SceneItems &items,
      std::vector<EnvironmentLightInstance> &environmentLights) const;

 private:
  Object                                 *m_owner{};
  helium::ChangeObserverPtr<ObjectArray> m_surface_data;
  helium::ChangeObserverPtr<ObjectArray> m_volume_data;
  helium::ChangeObserverPtr<ObjectArray> m_light_data;
};

struct Group : public SceneObject
{
 public:
  explicit Group(RPRGlobalState *s);
  void commitParameters() override;
  box3 bounds() const override;

  const GroupContent &content() const;

 private:
  GroupContent m_content;
};

} // namespace rpr
ANARI_TYPEFOR_SPECIALIZATION(rpr::Group *, ANARI_GROUP)
} // namespace anari
