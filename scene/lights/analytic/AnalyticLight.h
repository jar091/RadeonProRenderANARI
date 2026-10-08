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

#include "../Light.h"

namespace anari::rpr {

// Base of the lights which are RPR light objects
struct AnalyticLight : public Light
{
  explicit AnalyticLight(RPRGlobalState *s);

 protected:
  // sets the transformation of a new RPR light and attaches it to the scene
  void attachLight(rpr_scene scene, rpr_light light, const mat4 &transform, SceneItems &items);

  // solid angle of a cone with the given apex angle
  static float calculateSteradianAngle(float angle);
};

} // namespace anari::rpr
