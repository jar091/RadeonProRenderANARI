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

#include "Light.h"
// specific types
#include "analytic/DirectionalLight.h"
#include "analytic/HdriLight.h"
#include "analytic/PointLight.h"
#include "analytic/RingLight.h"
#include "analytic/SpotLight.h"
#include "area/QuadLight.h"

namespace anari {
namespace rpr {

Light::Light(RPRGlobalState *s) : SceneObject(ANARI_LIGHT, s) {}

Light *Light::createInstance(std::string_view type, RPRGlobalState *s)
{
  if (type == "quad")
  {
    return new QuadLight(s);
  }
  if (type == "directional")
  {
    return new DirectionalLight(s);
  }
  if (type == "point")
  {
    return new PointLight(s);
  }
  if (type == "spot")
  {
    return new SpotLight(s);
  }
  if (type == "hdri")
  {
    return new HdriLight(s);
  }
  if (type == "ring")
  {
    return new RingLight(s);
  }
  return (Light *)new UnknownObject(ANARI_LIGHT, type, s);
}

void Light::commitParameters()
{
  m_visible = getParam<bool>("visible", true);
  m_color   = getParam<float3>("color", float3(1.f));
}

bool Light::isEnvironmentLight() const
{
  return false;
}

bool Light::visible() const
{
  return m_visible;
}

} // namespace rpr

ANARI_TYPEFOR_DEFINITION(rpr::Light *);

} // namespace anari
