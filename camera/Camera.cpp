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

#include "Camera.h"
// specific types
#include "Orthographic.h"
#include "Perspective.h"

namespace anari {
namespace rpr {

Camera::Camera(RPRGlobalState *s) : Object(ANARI_CAMERA, s) {}

Camera *Camera::createInstance(std::string_view type, RPRGlobalState *s)
{
  if (type == "perspective")
  {
    return new Perspective(s);
  }
  if (type == "orthographic")
  {
    return new Orthographic(s);
  }
  // an unknown camera must not break the application: a frame without valid camera can not be rendered
  auto *camera = new Perspective(s);
  camera->reportMessage(ANARI_SEVERITY_WARNING,
      "unsupported camera subtype '%s', a perspective camera is used instead", std::string(type).c_str());
  return camera;
}

void Camera::commitParameters()
{
  m_position  = getParam<float3>("position", float3(0.f));
  m_direction = safeNormalize(getParam<float3>("direction", float3(0.f, 0.f, -1.f)), float3(0.f, 0.f, -1.f));
  m_up        = safeNormalize(getParam<float3>("up", float3(0.f, 1.f, 0.f)), float3(0.f, 1.f, 0.f));
  m_aspect    = getParam<float>("aspect", 1.f);
  m_near      = getParam<float>("near", 0.f);
  m_far       = getParam<float>("far", 0.f);

  mat4 transform(linalg::identity);
  if (getParam("transform", ANARI_FLOAT32_MAT4, &transform))
  { // transform is applied to camera pos, direction and up
    m_position  = xfmPoint(transform, m_position);
    m_direction = safeNormalize(xfmVec(transform, m_direction), m_direction);
    m_up        = safeNormalize(xfmVec(transform, m_up), m_up);
  }

  if (!(m_aspect > 0.f))
    m_aspect = 1.f;

  box2 region(float2(0.f), float2(1.f));
  if (getParam("imageRegion", ANARI_FLOAT32_BOX2, &region)
      && (region.lower != float2(0.f) || region.upper != float2(1.f)))
  {
    reportMessage(ANARI_SEVERITY_WARNING, "camera parameter 'imageRegion' is not supported");
  }
}

void Camera::finalize()
{
  if (!m_camera)
  {
    CHECK(rprContextCreateCamera(deviceState()->context, &m_camera))
    if (!m_camera)
      return;
  }

  // the up vector must not be parallel to the direction
  float3 up = m_up;
  if (linalg::length(linalg::cross(m_direction, up)) < 1e-6f)
  {
    float3 unused;
    makeBasis(m_direction, up, unused);
  }

  const float3 at = m_position + m_direction;
  CHECK(rprCameraLookAt(
      m_camera, m_position.x, m_position.y, m_position.z, at.x, at.y, at.z, up.x, up.y, up.z))

  if (m_near > 0.f)
  {
    CHECK(rprCameraSetNearPlane(m_camera, m_near))
  }
  if (m_far > 0.f)
  {
    CHECK(rprCameraSetFarPlane(m_camera, m_far))
  }
}

bool Camera::isValid() const
{
  return m_camera != nullptr;
}

void Camera::attachToScene(rpr_scene scene)
{
  CHECK(rprSceneSetCamera(scene, m_camera))
}

Camera::~Camera()
{
  deviceState()->deleteLater(m_camera, DeleteOrder::SCENE_ITEM);
}

} // namespace rpr

ANARI_TYPEFOR_DEFINITION(rpr::Camera *);

} // namespace anari
