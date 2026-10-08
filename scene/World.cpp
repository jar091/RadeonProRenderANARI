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

#include "World.h"
#include "../renderer/Renderer.h"
#include "../sampler/image/Image.h"
#include "geometry/Surface.h"
#include "Instance.h"
#include "lights/analytic/HdriLight.h"
#include "lights/Light.h"
#include "volume/Volume.h"

namespace anari {
namespace rpr {

World::World(RPRGlobalState *s) : SceneObject(ANARI_WORLD, s), m_content(this), m_instance_data(this) {}

World::~World()
{
  // the objects are detached from the scene, which must not happen while a frame is rendered
  std::lock_guard<std::recursive_mutex> lock(deviceState()->rprMutex);
  clearItems();
  clearEnvironment();
  deviceState()->deleteLater(m_scene, DeleteOrder::SCENE);
}

void World::commitParameters()
{
  m_content.commit();
  m_instance_data = getObjectArray(this, "instance");
}

box3 World::bounds() const
{
  box3 result = m_content.bounds();
  if (m_instance_data)
  {
    for (auto **object = m_instance_data->handlesBegin(); object != m_instance_data->handlesEnd(); ++object)
    {
      if (!*object || (*object)->type() != ANARI_INSTANCE || !(*object)->isValid())
        continue;
      const box3 b = ((Instance *)*object)->bounds();
      if (!isEmpty(b))
        result.extend(b);
    }
  }
  return result;
}

rpr_scene World::update(const Renderer &renderer)
{
  auto *state = deviceState();

  if (!m_scene)
  {
    CHECK(rprContextCreateScene(state->context, &m_scene))
    if (!m_scene)
      return nullptr;
  }

  const bool sceneChanged = state->objectUpdates.lastSceneChange >= m_last_scene_update;
  if (sceneChanged)
  {
    rebuildScene();
    m_last_scene_update = helium::newTimeStamp();
  }

  if (sceneChanged || &renderer != m_last_renderer || renderer.lastUpdated() >= m_last_environment_update)
  {
    rebuildEnvironment(renderer);
    m_last_renderer           = &renderer;
    m_last_environment_update = helium::newTimeStamp();
  }

  return m_scene;
}

bool World::hasVisibleEnvironment() const
{
  return m_environment_visible;
}

void World::rebuildScene()
{
  clearItems();
  m_environment_lights.clear();

  // surfaces, volumes and lights of the world itself
  m_content.attachToScene(
      m_scene, mat4(linalg::identity), UniformAttributeSet(), m_items, m_environment_lights);

  if (m_instance_data)
  {
    for (auto **object = m_instance_data->handlesBegin(); object != m_instance_data->handlesEnd(); ++object)
    {
      if (*object && (*object)->type() == ANARI_INSTANCE && (*object)->isValid())
        ((Instance *)*object)->attachToScene(m_scene, m_items, m_environment_lights);
    }
  }

  reportMessage(ANARI_SEVERITY_DEBUG, "RPR scene rebuilt with %zu shapes and %zu lights", m_items.shapes.size(),
      m_items.lights.size());
}

void World::clearItems()
{
  auto *state = deviceState();
  for (rpr_shape shape : m_items.shapes)
  {
    if (m_scene)
    {
      CHECK(rprSceneDetachShape(m_scene, shape))
    }
    state->deleteLater(shape, DeleteOrder::SCENE_ITEM);
  }
  for (rpr_light light : m_items.lights)
  {
    if (m_scene)
    {
      CHECK(rprSceneDetachLight(m_scene, light))
    }
    state->deleteLater(light, DeleteOrder::SCENE_ITEM);
  }
  for (rpr_shape shape : m_items.base_shapes)
  {
    if (m_scene)
    {
      CHECK(rprSceneDetachShape(m_scene, shape))
    }
  }
  m_items.shapes.clear();
  m_items.lights.clear();
  m_items.base_shapes.clear();
  m_items.graphs.clear();
}

void World::clearEnvironment()
{
  auto *state = deviceState();
  if (m_environment_light && m_scene)
  {
    CHECK(rprSceneDetachLight(m_scene, m_environment_light))
  }
  state->deleteLater(m_environment_light, DeleteOrder::SCENE_ITEM);
  state->deleteLater(m_refraction_light, DeleteOrder::SCENE_ITEM);
  state->deleteLater(m_background_light, DeleteOrder::SCENE_ITEM);
  state->deleteLater(m_environment_image, DeleteOrder::DATA);
  state->deleteLater(m_background_image, DeleteOrder::DATA);
  m_environment_light   = nullptr;
  m_refraction_light    = nullptr;
  m_background_light    = nullptr;
  m_environment_image   = nullptr;
  m_background_image    = nullptr;
  m_environment_visible = false;
}

// Orientation of the image of a RPR environment light relative to the frame of the ANARI light, which has
// the center of the image in the direction +x and its upper border in the direction +z (measured by
// rendering the environment in all directions). In addition the image of RPR is mirrored: the columns of
// the image are reversed when the image is created.
static const mat4 RPR_ENVIRONMENT_FRAME = mat4(
    float4(0.f, -1.f, 0.f, 0.f), float4(0.f, 0.f, 1.f, 0.f), float4(-1.f, 0.f, 0.f, 0.f), float4(0.f, 0.f, 0.f, 1.f));

void World::rebuildEnvironment(const Renderer &renderer)
{
  clearEnvironment();

  auto *state = deviceState();

  HdriLight *hdri = nullptr;
  mat4       hdriTransform(linalg::identity);
  if (!m_environment_lights.empty())
  {
    hdri          = m_environment_lights.front().light;
    hdriTransform = m_environment_lights.front().transform;
    if (m_environment_lights.size() > 1)
    {
      reportMessage(ANARI_SEVERITY_WARNING,
          "RadeonProRender supports a single environment light only, %zu hdri lights are ignored",
          m_environment_lights.size() - 1);
    }
  }

  // An hdri light replaces the ambient light of the renderer like in other ANARI devices: applications
  // set the ambient light as fallback for devices without environment lights.
  const float3 ambient    = hdri ? float3(0.f) : renderer.ambientRadiance();
  const bool   hasAmbient = ambient.x > 0.f || ambient.y > 0.f || ambient.z > 0.f;
  if (!hdri && !hasAmbient)
    return;

  // image of the environment: the image of the hdri light or the constant ambient light
  if (hdri)
  {
    const size_t        width  = hdri->width();
    const size_t        height = hdri->height();
    std::vector<float4> pixels(width * height);
    for (size_t y = 0; y < height; y++)
    {
      for (size_t x = 0; x < width; x++)
        pixels[y * width + x] = float4(hdri->radiance(width - 1 - x, y), 1.f);
    }
    m_environment_image = createImageFromData(state, pixels.data(), ANARI_FLOAT32_VEC4, width, height, true);
  } else
  {
    const float4 pixel(ambient, 1.f);
    m_environment_image = createImageFromData(state, &pixel, ANARI_FLOAT32_VEC4, 1, 1, false);
  }
  if (!m_environment_image)
    return;

  auto createLight = [&](rpr_image image, bool oriented) {
    rpr_light light = nullptr;
    CHECK(rprContextCreateEnvironmentLight(state->context, &light))
    if (!light)
      return light;
    CHECK(rprEnvironmentLightSetImage(light, image))
    CHECK(rprEnvironmentLightSetIntensityScale(light, 1.f))
    if (oriented)
    {
      // only the rotation of the instance is relevant for a light at infinity
      mat4 rotation = hdriTransform;
      rotation[3]   = float4(0.f, 0.f, 0.f, 1.f);
      const mat4 transform = linalg::mul(rotation, linalg::mul(hdri->orientation(), RPR_ENVIRONMENT_FRAME));
      CHECK(rprLightSetTransform(light, false, &transform[0][0]))
    }
    return light;
  };

  m_environment_light = createLight(m_environment_image, hdri != nullptr);
  if (!m_environment_light)
    return;

  // The environment illuminates the scene, but only a visible hdri light is seen by the camera. In all
  // other cases the background of RPR is black: the background of the renderer is the background image
  // of the scene, see Frame. The black background would also be seen through refractive surfaces, they
  // get the environment again.
  m_environment_visible = hdri && hdri->visible();
  if (!m_environment_visible)
  {
    const float4 black(0.f, 0.f, 0.f, 1.f);
    m_background_image = createImageFromData(state, &black, ANARI_FLOAT32_VEC4, 1, 1, false);
    if (m_background_image)
      m_background_light = createLight(m_background_image, false);
    m_refraction_light = createLight(m_environment_image, hdri != nullptr);
    if (m_background_light)
    {
      CHECK(rprEnvironmentLightSetEnvironmentLightOverride(
          m_environment_light, RPR_ENVIRONMENT_LIGHT_OVERRIDE_BACKGROUND, m_background_light))
    }
    if (m_refraction_light)
    {
      CHECK(rprEnvironmentLightSetEnvironmentLightOverride(
          m_environment_light, RPR_ENVIRONMENT_LIGHT_OVERRIDE_REFRACTION, m_refraction_light))
    }
  }

  CHECK(rprSceneAttachLight(m_scene, m_environment_light))
}

} // namespace rpr

ANARI_TYPEFOR_DEFINITION(rpr::World *);

} // namespace anari
