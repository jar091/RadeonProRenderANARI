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

#include "DirectionalLight.h"

namespace anari::rpr {

// 'radiant power' of a RPR directional light for an irradiance of 1 W/m^2
constexpr float RPR_DIRECTIONAL_POWER_PER_IRRADIANCE = 1.f;

DirectionalLight::DirectionalLight(RPRGlobalState *s) : AnalyticLight(s) {}

void DirectionalLight::commitParameters()
{
  Light::commitParameters();
  m_direction       = getParam<float3>("direction", float3(0.f, 0.f, -1.f));
  m_angularDiameter = getParam<float>("angularDiameter", 0.f);

  if (hasParam("irradiance") || !hasParam("radiance"))
  {
    m_irradiance = getParam<float>("irradiance", 1.f);
  } else
  {
    // radiance of the visible disk of the light. The disk of a light without angular diameter has no
    // area, the value is taken as irradiance then.
    const float radiance = getParam<float>("radiance", 1.f);
    m_irradiance = m_angularDiameter > 0.f ? radiance * calculateSteradianAngle(m_angularDiameter) : radiance;
  }
}

void DirectionalLight::attachToScene(rpr_scene scene, const mat4 &transform, SceneItems &items)
{
  rpr_light light = nullptr;
  CHECK(rprContextCreateDirectionalLight(deviceState()->context, &light))
  if (!light)
    return;
  const float3 power = m_color * m_irradiance * RPR_DIRECTIONAL_POWER_PER_IRRADIANCE;
  CHECK(rprDirectionalLightSetRadiantPower3f(light, power.x, power.y, power.z))
  // RPR limits the angle of the soft shadows to pi/4
  CHECK(rprDirectionalLightSetShadowSoftnessAngle(light, std::min(m_angularDiameter * 0.5f, pi / 4.f)))
  attachLight(scene, light, linalg::mul(transform, rotationFromDirection(m_direction)), items);
}

} // namespace anari::rpr
