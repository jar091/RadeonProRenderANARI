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

#include "../Object.h"
#include "../rpr_common.h"

namespace anari {
namespace rpr {

struct Camera : public Object
{
 public:
  explicit Camera(RPRGlobalState *s);
  ~Camera() override;

  static Camera *createInstance(std::string_view type, RPRGlobalState *s);

  void commitParameters() override;
  void finalize() override;
  bool isValid() const override;

  // makes the camera the one of the scene for a frame of the given size
  virtual void attachToScene(rpr_scene scene, const uint2 &frameSize);

  // value of the depth channel of a frame for a point of the scene
  virtual float depth(const float3 &point) const = 0;

 protected:
  // Sets position and orientation of the RPR camera. 'offset' moves the camera in its image plane (x to
  // the right, y upwards) without changing its direction.
  void setTransform(const float2 &offset = float2(0.f));

  // size and center of the image region
  float2 regionSize() const;
  float2 regionCenter() const;

  rpr_camera m_camera{};

  float3 m_position{0.f};
  float3 m_direction{0.f, 0.f, -1.f};
  float3 m_up{0.f, 1.f, 0.f};
  float  m_aspect{1.f};
  float  m_near{0.f};
  float  m_far{0.f};
  // 'imageRegion': the part of the image plane which a frame shows, in normalized coordinates of the
  // plane which is defined by the field of view (or the height) and 'aspect'. The origin is the lower
  // left corner.
  float2 m_region_lower{0.f};
  float2 m_region_upper{1.f};
};

} // namespace rpr

ANARI_TYPEFOR_SPECIALIZATION(rpr::Camera *, ANARI_CAMERA)

} // namespace anari
