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
#include "../Object.h"
#include "../rpr_common.h"

namespace anari {
namespace rpr {

struct Renderer : public Object
{
  explicit Renderer(RPRGlobalState *s);
  static Renderer *createInstance(std::string_view type, RPRGlobalState *s);

  void commitParameters() override;

  // sets the parameters of the RPR context which depend on the renderer
  virtual void attachToContext() const;

  // radiance of the ambient light
  float3 ambientRadiance() const;
  // time of the last change of the ambient light: the environment of a world has to be updated then,
  // but not when other parameters of the renderer change
  helium::TimeStamp ambientChanged() const;
  // background at a position of the image, (0, 0) is the lower left corner
  float4 background(float u, float v) const;
  bool   hasBackgroundImage() const;
  // samples per pixel of one frame
  int pixelSamples() const;

 protected:
  float4                             m_background_color{0.f, 0.f, 0.f, 1.f};
  helium::ChangeObserverPtr<Array2D> m_background_image;
  float                              m_ambient_radiance{0.f};
  float3                             m_ambient_color{1.f};
  helium::TimeStamp                  m_ambient_changed{0};
  int                                m_pixel_samples{1};
};

} // namespace rpr

ANARI_TYPEFOR_SPECIALIZATION(rpr::Renderer *, ANARI_RENDERER);

} // namespace anari
