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

#include "Renderer.h"
#include "PathTracerRenderer.h"
// specific types

namespace anari {
namespace rpr {

Renderer::Renderer(RPRGlobalState *s) : Object(ANARI_RENDERER, s), m_background_image(this) {}

Renderer *Renderer::createInstance(std::string_view type, RPRGlobalState *s)
{
  auto *renderer = new PathTracerRenderer(s);
  if (type != "default" && type != "pathtracer")
  {
    // an unknown renderer must not break the application: the path tracer is the only renderer
    renderer->reportMessage(ANARI_SEVERITY_WARNING,
        "unsupported renderer subtype '%s', the default renderer is used instead", std::string(type).c_str());
  }
  return renderer;
}

void Renderer::commitParameters()
{
  m_background_color = getParam<float4>("background", float4(0.f, 0.f, 0.f, 1.f));
  float3 color(0.f);
  if (getParam("background", ANARI_FLOAT32_VEC3, &color))
    m_background_color = float4(color, 1.f);
  m_background_image = getArray2D(this, "background");

  const float3 previousAmbient = ambientRadiance();
  m_ambient_radiance           = getParam<float>("ambientRadiance", 0.f);
  m_ambient_color              = getParam<float3>("ambientColor", float3(1.f));
  if (m_ambient_changed == 0 || ambientRadiance() != previousAmbient)
    m_ambient_changed = helium::newTimeStamp();
  m_pixel_samples    = std::max(getParam<int>("pixelSamples", 1), 1);
}

void Renderer::attachToContext() const
{
  CHECK(rprContextSetParameterByKey1u(deviceState()->context, RPR_CONTEXT_ITERATIONS, rpr_uint(m_pixel_samples)))
}

helium::TimeStamp Renderer::ambientChanged() const
{
  return m_ambient_changed;
}

float3 Renderer::ambientRadiance() const
{
  return linalg::max(m_ambient_color * m_ambient_radiance, float3(0.f));
}

int Renderer::pixelSamples() const
{
  return m_pixel_samples;
}

bool Renderer::hasBackgroundImage() const
{
  return m_background_image && m_background_image->size(0) > 0 && m_background_image->size(1) > 0;
}

float4 Renderer::background(float u, float v) const
{
  if (!hasBackgroundImage())
    return m_background_color;

  // bilinear interpolation, the centers of the outer pixels are the borders of the interpolation
  const Array2D *image  = m_background_image.get();
  const size_t   width  = image->size(0);
  const size_t   height = image->size(1);
  const float    x      = std::clamp(u * float(width) - 0.5f, 0.f, float(width - 1));
  const float    y      = std::clamp(v * float(height) - 0.5f, 0.f, float(height - 1));
  const size_t   x0     = size_t(x);
  const size_t   y0     = size_t(y);
  const size_t   x1     = std::min(x0 + 1, width - 1);
  const size_t   y1     = std::min(y0 + 1, height - 1);
  const float    fx     = x - float(x0);
  const float    fy     = y - float(y0);
  const float4   lower  = linalg::lerp(
      readAttributeValue(image, y0 * width + x0), readAttributeValue(image, y0 * width + x1), fx);
  const float4 upper = linalg::lerp(
      readAttributeValue(image, y1 * width + x0), readAttributeValue(image, y1 * width + x1), fx);
  return linalg::lerp(lower, upper, fy);
}

} // namespace rpr

ANARI_TYPEFOR_DEFINITION(rpr::Renderer *);

} // namespace anari
