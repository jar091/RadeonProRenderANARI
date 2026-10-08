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

#include "SpotLight.h"

namespace anari::rpr {

// The 'radiant power' of a RPR spot light is its radiant intensity in W/sr in the center of the cone
// (measured with a diffuse plane).

SpotLight::SpotLight(RPRGlobalState *s) : AnalyticLight(s) {}

void SpotLight::commitParameters()
{
  Light::commitParameters();
  m_position     = getParam<float3>("position", float3(0.f));
  m_direction    = getParam<float3>("direction", float3(0.f, 0.f, -1.f));
  m_openingAngle = std::clamp(getParam<float>("openingAngle", pi), 0.f, 2.f * pi);
  m_falloffAngle = std::clamp(getParam<float>("falloffAngle", 0.1f), 0.f, m_openingAngle * 0.5f);

  if (hasParam("intensity") || !hasParam("power"))
  {
    m_intensity = getParam<float>("intensity", 1.f);
  } else
  {
    // the power is emitted into the cone
    const float solidAngle = calculateSteradianAngle(m_openingAngle);
    m_intensity            = solidAngle > 0.f ? getParam<float>("power", 1.f) / solidAngle : 0.f;
  }
}

void SpotLight::attachToScene(rpr_scene scene, const mat4 &transform, SceneItems &items)
{
  rpr_light light = nullptr;
  CHECK(rprContextCreateSpotLight(deviceState()->context, &light))
  if (!light)
    return;
  const float3 power = m_color * m_intensity;
  CHECK(rprSpotLightSetRadiantPower3f(light, power.x, power.y, power.z))
  // RPR takes the angles between the axis and the inner and the outer border of the penumbra
  const float outerAngle = m_openingAngle * 0.5f;
  const float innerAngle = outerAngle - m_falloffAngle;
  CHECK(rprSpotLightSetConeShape(light, innerAngle, outerAngle))
  attachLight(scene, light, linalg::mul(transform, rotationFromDirection(m_direction, m_position)), items);
}

} // namespace anari::rpr
