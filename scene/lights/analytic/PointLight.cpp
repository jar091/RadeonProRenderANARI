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

#include "PointLight.h"

namespace anari::rpr {

// The 'radiant power' of a RPR point light is its radiant intensity in W/sr. The one of a RPR sphere light
// is the radiance of its surface in W/sr/m^2 (both were measured with a diffuse plane).

PointLight::PointLight(RPRGlobalState *s) : AnalyticLight(s) {}

void PointLight::commitParameters()
{
  Light::commitParameters();
  m_position = getParam<float3>("position", float3(0.f));
  m_radius   = std::max(getParam<float>("radius", 0.f), 0.f);

  if (hasParam("intensity") || (!hasParam("power") && !hasParam("radiance")))
  {
    m_intensity = getParam<float>("intensity", 1.f);
  } else if (hasParam("power"))
  {
    m_intensity = getParam<float>("power", 1.f) / (4.f * pi);
  } else
  {
    // radiance of the surface of the sphere
    m_intensity = getParam<float>("radiance", 1.f) * pi * m_radius * m_radius;
  }
}

void PointLight::attachToScene(rpr_scene scene, const mat4 &transform, SceneItems &items)
{
  rpr_light light = nullptr;
  if (m_radius == 0.f)
  {
    const float3 power = m_color * m_intensity;
    CHECK(rprContextCreatePointLight(deviceState()->context, &light))
    if (!light)
      return;
    CHECK(rprPointLightSetRadiantPower3f(light, power.x, power.y, power.z))
  } else
  {
    // a sphere with the radiance L has the intensity L * pi * r^2
    const float3 power = m_color * m_intensity / (pi * m_radius * m_radius);
    CHECK(rprContextCreateSphereLight(deviceState()->context, &light))
    if (!light)
      return;
    CHECK(rprSphereLightSetRadiantPower3f(light, power.x, power.y, power.z))
    CHECK(rprSphereLightSetRadius(light, m_radius))
  }
  attachLight(scene, light, linalg::mul(transform, linalg::translation_matrix(m_position)), items);
}

} // namespace anari::rpr
