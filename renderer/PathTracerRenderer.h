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

#include "Renderer.h"

namespace anari::rpr {

// The default renderer: the path tracer of the Northstar plugin
struct PathTracerRenderer : public Renderer
{
  explicit PathTracerRenderer(RPRGlobalState *s);
  void commitParameters() override;
  void attachToContext() const override;

 private:
  // values of the context parameters of RPR
  int   m_max_recursion{8};
  int   m_max_depth_diffuse{8};
  float m_radiance_clamp{0.f}; // 0: no clamp
};

} // namespace anari::rpr
