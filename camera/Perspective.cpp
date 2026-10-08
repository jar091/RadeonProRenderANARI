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

#include "Perspective.h"

namespace anari::rpr {

Perspective::Perspective(RPRGlobalState *s) : Camera(s) {}

void Perspective::commitParameters()
{
  Camera::commitParameters();

  m_sensor_height   = getParam<float>("sensorHeight", 24.f); // additional parameter
  m_fovy            = getParam<float>("fovy", pi / 3.f);
  m_focus_distance  = getParam<float>("focusDistance", 1.f);
  m_aperture_radius = getParam<float>("apertureRadius", 0.f);

  if (!(m_sensor_height > 0.f))
    m_sensor_height = 24.f;
  m_fovy = std::clamp(m_fovy, 1e-3f, pi - 1e-3f);
}

void Perspective::finalize()
{
  Camera::finalize();
  if (!m_camera)
    return;

  CHECK(rprCameraSetMode(m_camera, RPR_CAMERA_MODE_PERSPECTIVE))

  // sensor size and focal length are in millimeters
  const float sensorWidth = m_sensor_height * m_aspect;
  CHECK(rprCameraSetSensorSize(m_camera, sensorWidth, m_sensor_height))

  const float focalLength = m_sensor_height / (2.f * std::tan(m_fovy * 0.5f));
  CHECK(rprCameraSetFocalLength(m_camera, focalLength))

  if (m_aperture_radius > 0.f && m_focus_distance > 0.f)
  {
    // The diameter of the aperture is the focal length divided by the f-number. The focal length is
    // given in millimeters, the aperture radius in scene units (meters).
    const float fstop = focalLength / (2000.f * m_aperture_radius);
    CHECK(rprCameraSetFStop(m_camera, fstop))
    CHECK(rprCameraSetFocusDistance(m_camera, m_focus_distance))
  } else
  {
    // no depth of field
    CHECK(rprCameraSetFStop(m_camera, std::numeric_limits<float>::max()))
  }
}

float Perspective::depth(const float3 &point) const
{
  return linalg::length(point - m_position);
}

} // namespace anari::rpr
