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

#include "SpatialField.h"
#include "StucturedRegularSpatialField.h"

namespace anari {
namespace rpr {

SpatialField::SpatialField(RPRGlobalState *s) : SceneObject(ANARI_SPATIAL_FIELD, s) {}

SpatialField *SpatialField::createInstance(std::string_view type, RPRGlobalState *s)
{
  if (type == "structuredRegular")
  {
    return new StructuredRegularSpatialField(s);
  }
  return (SpatialField *)new UnknownObject(ANARI_SPATIAL_FIELD, type, s);
}

uint32_t SpatialField::resamplingFactor(const uint3 &size, uint32_t targetSize)
{
  const uint32_t maxSize = std::max(size.x, std::max(size.y, size.z));
  if (maxSize <= 1 || maxSize >= targetSize)
    return 1;
  return std::max((targetSize - 1) / (maxSize - 1), 1u);
}

std::vector<float> SpatialField::resample(uint32_t factor, uint3 &resampledSize) const
{
  const uint3 fieldSize = size();
  resampledSize         = (fieldSize - 1u) * factor + 1u;

  auto index = [&](uint32_t x, uint32_t y, uint32_t z) {
    return size_t(x) + size_t(fieldSize.x) * (size_t(y) + size_t(fieldSize.y) * size_t(z));
  };
  auto lerp = [](float a, float b, float t) { return a + (b - a) * t; };

  std::vector<float> values(size_t(resampledSize.x) * size_t(resampledSize.y) * size_t(resampledSize.z));
  size_t             i = 0;
  for (uint32_t z = 0; z < resampledSize.z; z++)
  {
    const uint32_t z0 = std::min(z / factor, fieldSize.z - 1), z1 = std::min(z0 + 1, fieldSize.z - 1);
    const float    fz = float(z - z0 * factor) / float(factor);
    for (uint32_t y = 0; y < resampledSize.y; y++)
    {
      const uint32_t y0 = std::min(y / factor, fieldSize.y - 1), y1 = std::min(y0 + 1, fieldSize.y - 1);
      const float    fy = float(y - y0 * factor) / float(factor);
      for (uint32_t x = 0; x < resampledSize.x; x++, i++)
      {
        if (factor == 1)
        {
          values[i] = value(index(x, y, z));
          continue;
        }
        const uint32_t x0 = std::min(x / factor, fieldSize.x - 1), x1 = std::min(x0 + 1, fieldSize.x - 1);
        const float    fx = float(x - x0 * factor) / float(factor);
        const float c00 = lerp(value(index(x0, y0, z0)), value(index(x1, y0, z0)), fx);
        const float c10 = lerp(value(index(x0, y1, z0)), value(index(x1, y1, z0)), fx);
        const float c01 = lerp(value(index(x0, y0, z1)), value(index(x1, y0, z1)), fx);
        const float c11 = lerp(value(index(x0, y1, z1)), value(index(x1, y1, z1)), fx);
        values[i]       = lerp(lerp(c00, c10, fy), lerp(c01, c11, fy), fz);
      }
    }
  }
  return values;
}

} // namespace rpr

ANARI_TYPEFOR_DEFINITION(rpr::SpatialField *);

} // namespace anari
