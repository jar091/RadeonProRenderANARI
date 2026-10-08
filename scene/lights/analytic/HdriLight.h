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

#include "AnalyticLight.h"

namespace anari::rpr {

// Environment light with an image in equirectangular projection. The RPR environment light is created
// by the world, because it also contains the ambient light of the renderer.
struct HdriLight : public AnalyticLight
{
  explicit HdriLight(RPRGlobalState *s);

  void commitParameters() override;
  void finalize() override;
  bool isValid() const override;

  void attachToScene(rpr_scene scene, const mat4 &transform, SceneItems &items) override;
  bool isEnvironmentLight() const override;

  // size of the environment image
  size_t width() const;
  size_t height() const;
  // radiance of a pixel of the image: 'radiance' * 'color' * 'scale'
  float3 radiance(size_t x, size_t y) const;

  // rotation of the light: maps the local axes of the image to world space
  mat4 orientation() const;

 private:
  helium::ChangeObserverPtr<Array2D> m_radiance;
  float3                             m_direction{1.f, 0.f, 0.f};
  float3                             m_up{0.f, 0.f, 1.f};
  float                              m_scale{1.f};
};

} // namespace anari::rpr
