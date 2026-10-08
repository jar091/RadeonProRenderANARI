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

#include "Group.h"
#include "geometry/Surface.h"
#include "lights/analytic/HdriLight.h"
#include "lights/Light.h"
#include "volume/Volume.h"

namespace anari {
namespace rpr {

// calls a function for all valid objects of an array of objects
template <typename OBJECT_T, typename FCN_T>
static void forEachValidObject(const ObjectArray *array, ANARIDataType type, FCN_T &&fcn)
{
  if (!array)
    return;
  for (auto **object = array->handlesBegin(); object != array->handlesEnd(); ++object)
  {
    if (*object && (*object)->type() == type && (*object)->isValid())
      fcn((OBJECT_T *)*object);
  }
}

// GroupContent definitions ///////////////////////////////////////////////////

GroupContent::GroupContent(Object *owner)
    : m_owner(owner), m_surface_data(owner), m_volume_data(owner), m_light_data(owner)
{}

void GroupContent::commit()
{
  m_surface_data = getObjectArray(m_owner, "surface");
  m_volume_data  = getObjectArray(m_owner, "volume");
  m_light_data   = getObjectArray(m_owner, "light");
}

box3 GroupContent::bounds() const
{
  box3 result = emptyBox();
  forEachValidObject<Surface>(m_surface_data.get(), ANARI_SURFACE, [&](Surface *surface) {
    const box3 b = surface->bounds();
    if (!isEmpty(b))
      result.extend(b);
  });
  forEachValidObject<Volume>(m_volume_data.get(), ANARI_VOLUME, [&](Volume *volume) {
    const box3 b = volume->bounds();
    if (!isEmpty(b))
      result.extend(b);
  });
  return result;
}

void GroupContent::attachToScene(rpr_scene scene, const mat4 &transform,
    const UniformAttributeSet &instanceAttributes, SceneItems &items,
    std::vector<EnvironmentLightInstance> &environmentLights) const
{
  forEachValidObject<Light>(m_light_data.get(), ANARI_LIGHT, [&](Light *light) {
    if (light->isEnvironmentLight())
      environmentLights.push_back({(HdriLight *)light, transform});
    else
      light->attachToScene(scene, transform, items);
  });
  forEachValidObject<Surface>(m_surface_data.get(), ANARI_SURFACE,
      [&](Surface *surface) { surface->attachToScene(scene, transform, instanceAttributes, items); });
  forEachValidObject<Volume>(m_volume_data.get(), ANARI_VOLUME,
      [&](Volume *volume) { volume->attachToScene(scene, transform, items); });
}

// Group definitions //////////////////////////////////////////////////////////

Group::Group(RPRGlobalState *s) : SceneObject(ANARI_GROUP, s), m_content(this) {}

void Group::commitParameters()
{
  m_content.commit();
}

box3 Group::bounds() const
{
  return m_content.bounds();
}

const GroupContent &Group::content() const
{
  return m_content;
}

} // namespace rpr
ANARI_TYPEFOR_DEFINITION(rpr::Group *);
} // namespace anari
