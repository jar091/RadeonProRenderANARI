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

#include "AnalyticLight.h"

namespace anari::rpr {

AnalyticLight::AnalyticLight(RPRGlobalState *s) : Light(s) {}

void AnalyticLight::attachLight(rpr_scene scene, rpr_light light, const mat4 &transform, SceneItems &items)
{
  if (!light)
    return;
  CHECK(rprLightSetTransform(light, false, &transform[0][0]))
  CHECK(rprSceneAttachLight(scene, light))
  items.lights.push_back(light);
}

float AnalyticLight::calculateSteradianAngle(float angle)
{
  return 2.f * pi * (1.f - std::cos(angle / 2.f));
}

} // namespace anari::rpr
