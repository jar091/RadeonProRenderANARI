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

  void attachToScene(rpr_scene scene);

  // value of the depth channel of a frame for a point of the scene
  virtual float depth(const float3 &point) const = 0;

 protected:
  rpr_camera m_camera{};

  float3 m_position{0.f};
  float3 m_direction{0.f, 0.f, -1.f};
  float3 m_up{0.f, 1.f, 0.f};
  float  m_aspect{1.f};
  float  m_near{0.f};
  float  m_far{0.f};
};

} // namespace rpr

ANARI_TYPEFOR_SPECIALIZATION(rpr::Camera *, ANARI_CAMERA)

} // namespace anari
