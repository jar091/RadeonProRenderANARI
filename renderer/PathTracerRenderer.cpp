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

// How Northstar counts the depth of a path was measured (RPR 3.1.7, see README.MD):
// - RPR_CONTEXT_MAX_RECURSION is the number of segments of a path after the point which the camera
//   sees, the segment to a light included: 1 is direct light only, n + 1 allows n bounces. A surface
//   with transparency which a path passes counts as a bounce too.
// - RPR_CONTEXT_MAX_DEPTH_DIFFUSE counts the diffuse bounces the same way: n + 1 allows n of them.
// - RPR_CONTEXT_MAX_DEPTH_GLOSSY, _REFRACTION, _GLOSSY_REFRACTION and _SHADOW have no effect.
// - RPR_CONTEXT_RADIANCE_CLAMP limits every color channel of the light which a path collects at a
//   bounce (not at the point which the camera sees: direct light is never clamped).
// The parameters 'maxBounce' and 'maxDiffuseBounce' count bounces like Cycles does (0: direct light
// only), 'maxRayDepth' is the value of RPR_CONTEXT_MAX_RECURSION as it is.

void PathTracerRenderer::commitParameters()
{
  Renderer::commitParameters();

  const int maxDepth = 50;
  m_max_recursion    = std::clamp(getParam<int>("maxRayDepth", 8), 1, maxDepth);
  int bounces        = 0;
  if (getParam("maxBounce", ANARI_INT32, &bounces))
    m_max_recursion = std::clamp(bounces, 0, maxDepth - 1) + 1;
  // without limit for diffuse bounces the limit of the path applies
  m_max_depth_diffuse = m_max_recursion;
  if (getParam("maxDiffuseBounce", ANARI_INT32, &bounces))
    m_max_depth_diffuse = std::clamp(bounces, 0, maxDepth - 1) + 1;

  m_radiance_clamp = getParam<float>("clampIndirect", getParam<float>("radianceClamp", 0.f));
}

void PathTracerRenderer::attachToContext() const
{
  Renderer::attachToContext();

  rpr_context context = deviceState()->context;
  CHECK(rprContextSetParameterByKey1u(context, RPR_CONTEXT_MAX_RECURSION, rpr_uint(m_max_recursion)))
  CHECK(rprContextSetParameterByKey1u(context, RPR_CONTEXT_MAX_DEPTH_DIFFUSE, rpr_uint(m_max_depth_diffuse)))
  // these limits have no effect in Northstar, they get the limit of the path
  const rpr_uint depth = rpr_uint(m_max_recursion);
  CHECK(rprContextSetParameterByKey1u(context, RPR_CONTEXT_MAX_DEPTH_GLOSSY, depth))
  CHECK(rprContextSetParameterByKey1u(context, RPR_CONTEXT_MAX_DEPTH_REFRACTION, depth))
  CHECK(rprContextSetParameterByKey1u(context, RPR_CONTEXT_MAX_DEPTH_GLOSSY_REFRACTION, depth))
  CHECK(rprContextSetParameterByKey1u(context, RPR_CONTEXT_MAX_DEPTH_SHADOW, depth))
  CHECK(rprContextSetParameterByKey1f(context, RPR_CONTEXT_RADIANCE_CLAMP,
      m_radiance_clamp > 0.f ? m_radiance_clamp : std::numeric_limits<float>::max()))
}

} // namespace anari::rpr
