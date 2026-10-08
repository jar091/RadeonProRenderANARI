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

#include "Image3D.h"
// std
#include <vector>

namespace anari::rpr {

Image3D::Image3D(RPRGlobalState *s) : Image(s), m_data(this) {}

void Image3D::commitParameters()
{
  Image::commitParameters();
  m_wrap_mode_u = processWrap(getParamString("wrapMode1", "clampToEdge"));
  m_wrap_mode_v = processWrap(getParamString("wrapMode2", "clampToEdge"));
  m_wrap_mode_w = processWrap(getParamString("wrapMode3", "clampToEdge"));
  m_data        = getArray3D(this, "image");
}

void Image3D::finalize()
{
  clearImage();

  if (!m_data)
  {
    reportMessage(ANARI_SEVERITY_WARNING, "missing required parameter 'image' on image3D sampler");
    return;
  }

  const size_t width  = m_data->size(0);
  const size_t height = m_data->size(1);
  const size_t depth  = m_data->size(2);
  if (width == 0 || height == 0 || depth == 0)
    return;

  // arrange the slices in a grid of tiles which is about square
  const size_t tilesX = size_t(std::ceil(std::sqrt(double(depth) * double(height) / double(width))));
  const size_t tilesY = (depth + tilesX - 1) / tilesX;
  constexpr size_t MAX_IMAGE_SIZE = 16384;
  if (width * tilesX > MAX_IMAGE_SIZE || height * tilesY > MAX_IMAGE_SIZE)
  {
    reportMessage(ANARI_SEVERITY_WARNING, "the image of the image3D sampler is too big, the sampler is ignored");
    return;
  }

  const size_t        atlasWidth  = width * tilesX;
  const size_t        atlasHeight = height * tilesY;
  std::vector<float4> atlas(atlasWidth * atlasHeight, float4(0.f, 0.f, 0.f, 1.f));
  for (size_t z = 0; z < depth; z++)
  {
    const size_t tileX = z % tilesX;
    const size_t tileY = z / tilesX;
    for (size_t y = 0; y < height; y++)
    {
      float4 *dst = atlas.data() + (tileY * height + y) * atlasWidth + tileX * width;
      for (size_t x = 0; x < width; x++)
        dst[x] = readAttributeValue(m_data.get(), (z * height + y) * width + x);
    }
  }

  setImage(atlas.data(), ANARI_FLOAT32_VEC4, atlasWidth, atlasHeight);
  m_size  = float3(float(width), float(height), float(depth));
  m_tiles = float2(float(tilesX), float(tilesY));
}

MaterialValue Image3D::generateMaterial(MaterialGraph &graph) const
{
  if (!m_image)
    return MaterialValue(DEFAULT_ATTRIBUTE_VALUE);

  const MaterialValue coordinates =
      applyTransform(graph, m_input_transform, m_input_offset, graph.attribute(m_input_attribute));

  const bool nearest = m_filter == RPR_IMAGE_FILTER_TYPE_NEAREST;

  // All values of the following calculations are scalars, which are stored in all components of a node.
  auto component = [&](int index) {
    float4 mask(0.f);
    mask[index] = 1.f;
    return graph.arithmetic(RPR_MATERIAL_NODE_OP_DOT4, coordinates, MaterialValue(mask));
  };
  auto floor = [&](const MaterialValue &value) {
    return graph.arithmetic(RPR_MATERIAL_NODE_OP_FLOOR, value, MaterialValue(0.f));
  };
  auto clamp = [&](const MaterialValue &value, float lower, float upper) {
    return graph.arithmetic(RPR_MATERIAL_NODE_OP_MIN,
        graph.arithmetic(RPR_MATERIAL_NODE_OP_MAX, value, MaterialValue(lower)), MaterialValue(upper));
  };
  // coordinate in [0, 1] depending on the wrap mode ('mirrorRepeat' is handled like 'repeat')
  auto wrap = [&](const MaterialValue &value, rpr_image_wrap_type mode) {
    return mode == RPR_IMAGE_WRAP_TYPE_CLAMP_TO_EDGE ? value : graph.sub(value, floor(value));
  };
  // Coordinate inside of a tile. The interpolation of RPR must not read texels of the adjacent tiles, so
  // the coordinate is limited to the centers of the outer texels.
  auto tileCoordinate = [&](const MaterialValue &value, float size) {
    MaterialValue result = clamp(value, 0.5f / size, 1.f - 0.5f / size);
    if (nearest)
      result = graph.mul(graph.add(floor(graph.mul(result, MaterialValue(size))), MaterialValue(0.5f)),
          MaterialValue(1.f / size));
    return result;
  };

  const MaterialValue u = tileCoordinate(wrap(component(0), m_wrap_mode_u), m_size.x);
  const MaterialValue v = tileCoordinate(wrap(component(1), m_wrap_mode_v), m_size.y);
  const MaterialValue w = wrap(component(2), m_wrap_mode_w);

  // samples the tile of a slice
  auto sampleSlice = [&](const MaterialValue &slice) {
    // row and column of the tile, the small offset avoids wrong rows caused by rounding errors
    const MaterialValue row = floor(graph.mul(graph.add(slice, MaterialValue(0.5f)), MaterialValue(1.f / m_tiles.x)));
    const MaterialValue column = graph.sub(slice, graph.mul(row, MaterialValue(m_tiles.x)));
    const MaterialValue atlasU = graph.mul(graph.add(u, column), MaterialValue(1.f / m_tiles.x));
    const MaterialValue atlasV = graph.mul(graph.add(v, row), MaterialValue(1.f / m_tiles.y));
    const MaterialValue uv     = graph.add(graph.mul(atlasU, MaterialValue(float4(1.f, 0.f, 0.f, 0.f))),
            graph.mul(atlasV, MaterialValue(float4(0.f, 1.f, 0.f, 0.f))));
    return generateImageTexture(graph, uv, RPR_IMAGE_WRAP_TYPE_CLAMP_TO_EDGE, RPR_IMAGE_WRAP_TYPE_CLAMP_TO_EDGE);
  };

  MaterialValue result;
  if (nearest)
  {
    result = sampleSlice(clamp(floor(graph.mul(w, MaterialValue(m_size.z))), 0.f, m_size.z - 1.f));
  } else
  {
    // the samples of the slices are in the centers of the slices
    const MaterialValue position =
        clamp(graph.sub(graph.mul(w, MaterialValue(m_size.z)), MaterialValue(0.5f)), 0.f, m_size.z - 1.f);
    const MaterialValue lower    = floor(position);
    const MaterialValue upper    = graph.arithmetic(
        RPR_MATERIAL_NODE_OP_MIN, graph.add(lower, MaterialValue(1.f)), MaterialValue(m_size.z - 1.f));
    const MaterialValue fraction = graph.sub(position, lower);
    const MaterialValue first    = sampleSlice(lower);
    const MaterialValue second   = sampleSlice(upper);
    result = graph.add(first, graph.mul(graph.sub(second, first), fraction));
  }

  return applyTransform(graph, m_output_transform, m_output_offset, result);
}

} // namespace anari::rpr
