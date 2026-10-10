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

  // the frame shows the image region of the plane with the size 'height * aspect' x 'height'
  const float2 regionSize = this->regionSize();
  CHECK(rprCameraSetOrthoHeight(m_camera, m_height * regionSize.y))
  CHECK(rprCameraSetOrthoWidth(m_camera, m_height * m_aspect * regionSize.x))

  // the position of the camera depends on the size of the frame, see attachToScene()
  m_frame_size = uint2(0u);
}

void Orthographic::attachToScene(rpr_scene scene, const uint2 &frameSize)
{
  if (m_camera && frameSize != m_frame_size && frameSize.x > 0 && frameSize.y > 0)
  {
    m_frame_size = frameSize;
    // The lens shift of RPR has no effect on orthographic cameras. The camera is moved in its image
    // plane to the center of the image region instead, which is exact for parallel rays.
    // In addition the image of an orthographic RPR camera is half a pixel off (measured: the center of
    // the view is at the pixel position (width / 2 - 0.5, height / 2 - 0.5)), which is corrected here.
    const float2 planeSize(m_height * m_aspect, m_height);
    const float2 pixelSize = planeSize * regionSize() / float2(float(frameSize.x), float(frameSize.y));
    setTransform((regionCenter() - float2(0.5f)) * planeSize + pixelSize * 0.5f);
  }
  Camera::attachToScene(scene, frameSize);
}

float Orthographic::depth(const float3 &point) const
{
  return linalg::dot(point - m_position, m_direction);
}

} // namespace anari::rpr
