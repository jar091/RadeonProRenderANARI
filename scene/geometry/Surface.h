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

#include "../../material/Material.h"
#include "../../rpr_common.h"
#include "../RayVisibility.h"
#include "../SceneObject.h"
#include "Geometry.h"
// std
#include <memory>
#include <set>
#include <unordered_map>
#include <vector>

namespace anari {
namespace rpr {

// RPR instance of the base shape of a geometry
struct SurfaceInstance
{
  rpr_shape     base{nullptr};
  rpr_shape     instance{nullptr};
  RayVisibility visibility; // the flags which the instance has
  helium::TimeStamp material{0}; // MaterialGraph::id of its material
};

// RPR objects which are part of a scene. They are created when the scene of a world is updated.
struct SceneItems
{
  // The instances of the surfaces. The instances of the previous scene are used again (see 'pool'):
  // creating the instances of a big scene takes time and the primvars of a mesh have to be set again
  // for instances which are created after it was rendered (see Geometry::instancesAttached()).
  std::vector<SurfaceInstance> instances;
  // the instances of the previous scene which are not used yet, by base shape. They are still attached
  // to the scene.
  std::unordered_multimap<rpr_shape, SurfaceInstance> pool;
  // identifies the update of the scene which is in progress
  helium::TimeStamp build{0};

  // all other shapes (area lights, volumes), they are created again with every update
  std::vector<rpr_shape> shapes;
  std::vector<rpr_light> lights;
  // Base shapes of the geometries. They are owned by the geometries, but they have to be part of the
  // scene as long as their instances are.
  std::set<rpr_shape> base_shapes;
  std::set<rpr_shape> previous_base_shapes; // the ones of the previous scene, still attached to the scene
  // material nodes which are used by the shapes of this scene only
  std::vector<std::unique_ptr<MaterialGraph>> graphs;
};

struct Surface : public SceneObject
{
  explicit Surface(RPRGlobalState *s);

  void commitParameters() override;
  void finalize() override;
  bool isValid() const override;
  box3 bounds() const override;

  // Adds an instance of the geometry with the material of the surface to the scene: an instance of the
  // previous scene with the same geometry is used again, otherwise a new one is created.
  void attachToScene(rpr_scene scene, const mat4 &transform, const UniformAttributeSet &instanceAttributes,
      const RayVisibility &visibility, SceneItems &items);

 private:
  helium::IntrusivePtr<Geometry> m_geometry;
  helium::IntrusivePtr<Material> m_material;
  // used if the surface has no valid material
  helium::IntrusivePtr<Material> m_default_material;
  bool                           m_visible{true};
};

} // namespace rpr

ANARI_TYPEFOR_SPECIALIZATION(rpr::Surface *, ANARI_SURFACE);

} // namespace anari
