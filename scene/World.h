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

struct Instance;
struct Renderer;

// The world owns a RPR scene. The scene is built from the objects of the world before a frame is rendered
// if one of the objects has changed.
struct World : public SceneObject
{
  explicit World(RPRGlobalState *s);
  ~World() override;

  void commitParameters() override;
  box3 bounds() const override;

  // Brings the RPR scene up to date and returns it. The ambient light of the renderer is part of the
  // environment light of the scene.
  rpr_scene update(const Renderer &renderer);

  // the environment light is seen by the camera, it replaces the background of the renderer
  bool hasVisibleEnvironment() const;

 private:
  void rebuildScene();
  void rebuildEnvironment(const Renderer &renderer);
  void clearItems();
  void clearEnvironment();

  GroupContent                           m_content;
  helium::ChangeObserverPtr<ObjectArray> m_instance_data;

  rpr_scene  m_scene{};
  SceneItems m_items;

  std::vector<EnvironmentLightInstance> m_environment_lights;
  rpr_light                             m_environment_light{};
  rpr_image                             m_environment_image{};
  rpr_light                             m_background_light{};
  rpr_image                             m_background_image{};
  rpr_light                             m_refraction_light{};
  bool                                  m_environment_visible{false};

  helium::TimeStamp m_last_scene_update{0};
  helium::TimeStamp m_last_environment_update{0};
  const Renderer   *m_last_renderer{nullptr};
};

} // namespace rpr

ANARI_TYPEFOR_SPECIALIZATION(rpr::World *, ANARI_WORLD);

} // namespace anari
