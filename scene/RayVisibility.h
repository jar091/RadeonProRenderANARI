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

#include "../rpr_common.h"

namespace anari::rpr {

// The kinds of rays which see the surfaces of an instance (the parameters 'visibility.*' of an instance,
// with the meaning of the ray visibility of an object in Cycles).
struct RayVisibility
{
  bool camera{true};
  bool diffuse{true};
  bool glossy{true};
  bool transmission{true};
  bool volumeScatter{true};
  bool shadow{true};

  // visible to all kinds of rays
  bool all() const
  {
    return camera && diffuse && glossy && transmission && volumeScatter && shadow;
  }

  // visible to at least one kind of rays
  bool any() const
  {
    return camera || diffuse || glossy || transmission || volumeScatter || shadow;
  }

  bool operator==(const RayVisibility &other) const
  {
    return camera == other.camera && diffuse == other.diffuse && glossy == other.glossy
        && transmission == other.transmission && volumeScatter == other.volumeScatter && shadow == other.shadow;
  }
};

// Sets the visibility flags of a RPR shape. What the flags of Northstar do was measured (RPR 3.1.7):
//   PRIMARY_ONLY_FLAG                            camera rays, also behind surfaces with transparency
//   SHADOW                                       shadow rays
//   DIFFUSE                                      rays reflected by diffuse lobes, this includes the light
//                                                which an emissive shape casts on diffuse surfaces
//   REFLECTION, GLOSSY_REFLECTION                rays reflected by specular lobes, sharp or rough: the
//                                                shape is hidden if one of the two flags is not set
//   REFRACTION, GLOSSY_REFRACTION, TRANSPARENT   refracted rays, sharp or rough: the shape is hidden if
//                                                one of the three flags is not set
// There is no flag for rays which are scattered by a volume: 'volumeScatter' has no effect here.
// The flags LIGHT (which hides a shape from camera rays and refracted rays) and RECEIVE_SHADOW are not
// used.
void setRayVisibility(const RPRGlobalState *state, rpr_shape shape, const RayVisibility &visibility);

} // namespace anari::rpr
