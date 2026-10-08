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

#include "PathTracerRenderer.h"

namespace anari::rpr {

PathTracerRenderer::PathTracerRenderer(RPRGlobalState *s) : Renderer(s) {}

void PathTracerRenderer::commitParameters()
{
  Renderer::commitParameters();
  m_max_ray_depth  = std::clamp(getParam<int>("maxRayDepth", 8), 1, 50);
  m_radiance_clamp = getParam<float>("radianceClamp", 0.f);
}

void PathTracerRenderer::attachToContext() const
{
  Renderer::attachToContext();

  rpr_context context = deviceState()->context;
  // the same limit for all kinds of bounces
  const rpr_uint depth = rpr_uint(m_max_ray_depth);
  CHECK(rprContextSetParameterByKey1u(context, RPR_CONTEXT_MAX_RECURSION, depth))
  CHECK(rprContextSetParameterByKey1u(context, RPR_CONTEXT_MAX_DEPTH_DIFFUSE, depth))
  CHECK(rprContextSetParameterByKey1u(context, RPR_CONTEXT_MAX_DEPTH_GLOSSY, depth))
  CHECK(rprContextSetParameterByKey1u(context, RPR_CONTEXT_MAX_DEPTH_REFRACTION, depth))
  CHECK(rprContextSetParameterByKey1u(context, RPR_CONTEXT_MAX_DEPTH_GLOSSY_REFRACTION, depth))
  CHECK(rprContextSetParameterByKey1u(context, RPR_CONTEXT_MAX_DEPTH_SHADOW, depth))
  CHECK(rprContextSetParameterByKey1f(context, RPR_CONTEXT_RADIANCE_CLAMP,
      m_radiance_clamp > 0.f ? m_radiance_clamp : std::numeric_limits<float>::max()))
}

} // namespace anari::rpr
