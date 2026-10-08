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

#include "Orthographic.h"

namespace anari::rpr {

Orthographic::Orthographic(RPRGlobalState *s) : Camera(s) {}

void Orthographic::commitParameters()
{
  Camera::commitParameters();

  // 'height' is the parameter of ANARI 1.0, 'orthoHeight' was used by older versions of the device
  m_height = getParam<float>("height", getParam<float>("orthoHeight", 1.f));
  if (!(m_height > 0.f))
    m_height = 1.f;
}

void Orthographic::finalize()
{
  Camera::finalize();
  if (!m_camera)
    return;

  CHECK(rprCameraSetMode(m_camera, RPR_CAMERA_MODE_ORTHOGRAPHIC))

  const float sensorHeight = 24.f;
  CHECK(rprCameraSetSensorSize(m_camera, sensorHeight * m_aspect, sensorHeight))

  CHECK(rprCameraSetOrthoHeight(m_camera, m_height))
  CHECK(rprCameraSetOrthoWidth(m_camera, m_height * m_aspect))
}

float Orthographic::depth(const float3 &point) const
{
  return linalg::dot(point - m_position, m_direction);
}

} // namespace anari::rpr
