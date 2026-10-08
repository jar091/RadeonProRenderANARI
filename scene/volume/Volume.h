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

#include "../SceneObject.h"
#include "../geometry/Surface.h"

namespace anari {
namespace rpr {

struct Volume : public SceneObject
{
 public:
  explicit Volume(RPRGlobalState *s);
  static Volume *createInstance(std::string_view type, RPRGlobalState *s);

  // creates the RPR shape of the volume for an instance and attaches it to the scene
  virtual void attachToScene(rpr_scene scene, const mat4 &transform, SceneItems &items) = 0;
};

} // namespace rpr

ANARI_TYPEFOR_SPECIALIZATION(rpr::Volume *, ANARI_VOLUME)

} // namespace anari
