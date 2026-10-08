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

#include "Image.h"
// std
#include <cstring>
#include <vector>

namespace anari::rpr {

// RPR images have their origin in the upper left corner, the first element of an ANARI image is the
// one with the texture coordinates (0, 0)
constexpr bool FLIP_IMAGE_ROWS = true;

// number of 8 bit components of an element type, 0 for all other types
static int numComponents8Bit(ANARIDataType type, bool &isSRGB)
{
  isSRGB = false;
  switch (type)
  {
  case ANARI_UFIXED8:
    return 1;
  case ANARI_UFIXED8_VEC2:
    return 2;
  case ANARI_UFIXED8_VEC3:
    return 3;
  case ANARI_UFIXED8_VEC4:
    return 4;
  case ANARI_UFIXED8_R_SRGB:
    isSRGB = true;
    return 1;
  case ANARI_UFIXED8_RA_SRGB:
    isSRGB = true;
    return 2;
  case ANARI_UFIXED8_RGB_SRGB:
    isSRGB = true;
    return 3;
  case ANARI_UFIXED8_RGBA_SRGB:
    isSRGB = true;
    return 4;
  default:
    return 0;
  }
}

// conversion of a float in [0, 1] to a 16 bit float
static uint16_t floatToHalf(float value)
{
  uint32_t bits = 0;
  std::memcpy(&bits, &value, sizeof(bits));
  const int32_t  exponent = int32_t((bits >> 23) & 0xffu) - 127 + 15;
  const uint32_t mantissa = bits & 0x7fffffu;
  if (exponent <= 0)
  {
    // zero and denormalized numbers
    if (exponent < -10)
      return 0;
    return uint16_t((((mantissa | 0x800000u) >> (1 - exponent)) + 0x1000u) >> 13);
  }
  if (exponent >= 31)
    return 0x7c00u;
  return uint16_t((uint32_t(exponent) << 10) + ((mantissa + 0x1000u) >> 13));
}

rpr_image createImageFromData(const RPRGlobalState *state, const void *data, ANARIDataType elementType,
    size_t width, size_t height, bool flipRows)
{
  auto deviceState = [&]() { return state; };

  if (!data || width == 0 || height == 0)
    return nullptr;

  rpr_image      image = nullptr;
  rpr_image_desc desc;
  desc.image_width       = rpr_uint(width);
  desc.image_height      = rpr_uint(height);
  desc.image_depth       = 0;
  desc.image_slice_pitch = 0;

  bool      isSRGB        = false;
  const int numComponents = numComponents8Bit(elementType, isSRGB);

  if (numComponents > 0 && isSRGB)
  {
    // 8 bit sRGB images become 16 bit float images with linear colors: 8 bits are not enough for the
    // dark linear colors and the gamma of RPR images is not the transfer function of sRGB.
    uint16_t colorTable[256], alphaTable[256];
    for (int i = 0; i < 256; i++)
    {
      colorTable[i] = floatToHalf(toneMap<ToneMapMode::FROM_SRGB>(float(i) / 255.f));
      alphaTable[i] = floatToHalf(float(i) / 255.f);
    }
    // the alpha value is the last component of the sRGB types with two and four components
    const int alphaComponent = numComponents == 2 ? 1 : numComponents == 4 ? 3 : -1;

    std::vector<uint16_t> pixels(width * height * 4);
    const auto           *src = (const uint8_t *)data;
    for (size_t y = 0; y < height; y++)
    {
      const uint8_t *srcRow = src + (flipRows ? height - 1 - y : y) * width * numComponents;
      uint16_t      *dstRow = pixels.data() + y * width * 4;
      for (size_t x = 0; x < width; x++)
      {
        uint16_t *dst = dstRow + x * 4;
        dst[0] = dst[1] = dst[2] = colorTable[0];
        dst[3]                   = alphaTable[255];
        for (int c = 0; c < numComponents; c++)
          dst[c] = c == alphaComponent ? alphaTable[srcRow[x * numComponents + c]]
                                       : colorTable[srcRow[x * numComponents + c]];
      }
    }
    desc.image_row_pitch = rpr_uint(width * 4 * sizeof(uint16_t));
    CHECK(rprContextCreateImage(state->context, {4, RPR_COMPONENT_TYPE_FLOAT16}, &desc, pixels.data(), &image))
    if (image)
    {
      CHECK(rprImageSetGamma(image, 1.f))
    }
  } else if (numComponents > 0)
  {
    // 8 bit images stay 8 bit images, missing components have the default values (0, 0, 0, 1)
    std::vector<uint8_t> pixels(width * height * 4);
    const auto          *src = (const uint8_t *)data;
    for (size_t y = 0; y < height; y++)
    {
      const uint8_t *srcRow = src + (flipRows ? height - 1 - y : y) * width * numComponents;
      uint8_t       *dstRow = pixels.data() + y * width * 4;
      for (size_t x = 0; x < width; x++)
      {
        uint8_t *dst = dstRow + x * 4;
        dst[0] = dst[1] = dst[2] = 0;
        dst[3]                   = 255;
        for (int c = 0; c < numComponents; c++)
          dst[c] = srcRow[x * numComponents + c];
      }
    }
    desc.image_row_pitch = rpr_uint(width * 4);
    CHECK(rprContextCreateImage(state->context, {4, RPR_COMPONENT_TYPE_UINT8}, &desc, pixels.data(), &image))
    if (image)
    {
      CHECK(rprImageSetGamma(image, 1.f))
    }
  } else
  {
    std::vector<float4> pixels(width * height);
    for (size_t y = 0; y < height; y++)
    {
      const size_t srcRow = flipRows ? height - 1 - y : y;
      for (size_t x = 0; x < width; x++)
        pixels[y * width + x] = readAsAttributeValueFlat(data, elementType, srcRow * width + x);
    }
    desc.image_row_pitch = rpr_uint(width * sizeof(float4));
    CHECK(rprContextCreateImage(state->context, {4, RPR_COMPONENT_TYPE_FLOAT32}, &desc, pixels.data(), &image))
    if (image)
    {
      CHECK(rprImageSetGamma(image, 1.f))
    }
  }
  return image;
}

Image::Image(RPRGlobalState *s) : Sampler(s) {}

Image::~Image()
{
  clearImage();
}

void Image::commitParameters()
{
  Sampler::commitParameters();

  m_input_transform = getParam<mat4>("inTransform", mat4(linalg::identity));
  m_input_offset    = getParam<float4>("inOffset", float4(0.f));
  m_filter          = getParamString("filter", "linear") == "nearest" ? RPR_IMAGE_FILTER_TYPE_NEAREST
                                                                      : RPR_IMAGE_FILTER_TYPE_LINEAR;
}

bool Image::isValid() const
{
  return m_image != nullptr;
}

void Image::setImage(const void *data, ANARIDataType elementType, size_t width, size_t height)
{
  clearImage();
  m_image      = createImageFromData(deviceState(), data, elementType, width, height, FLIP_IMAGE_ROWS);
  m_image_size = float2(float(width), float(height));
  if (m_image)
  {
    CHECK(rprImageSetFilter(m_image, m_filter))
  }
}

void Image::clearImage()
{
  deviceState()->deleteLater(m_image, DeleteOrder::DATA);
  m_image = nullptr;
}

MaterialValue Image::generateImageTexture(MaterialGraph &graph, const MaterialValue &uv,
    rpr_image_wrap_type wrapU, rpr_image_wrap_type wrapV) const
{
  rpr_material_node material = graph.createNode(RPR_MATERIAL_NODE_IMAGE_TEXTURE);
  if (!material)
    return MaterialValue(DEFAULT_ATTRIBUTE_VALUE);
  CHECK(rprMaterialNodeSetInputImageDataByKey(material, RPR_MATERIAL_INPUT_DATA, m_image))
  graph.setInput(material, RPR_MATERIAL_INPUT_UV, uv);
  graph.setInputU(material, RPR_MATERIAL_INPUT_WRAP_U, wrapU);
  graph.setInputU(material, RPR_MATERIAL_INPUT_WRAP_V, wrapV);
  return MaterialValue(material);
}

MaterialValue Image::generateMaterial(MaterialGraph &graph) const
{
  if (!m_image)
    return MaterialValue(DEFAULT_ATTRIBUTE_VALUE);

  MaterialValue uv =
      applyTransform(graph, m_input_transform, m_input_offset, graph.attribute(m_input_attribute));

  if (m_filter == RPR_IMAGE_FILTER_TYPE_NEAREST)
  {
    // Northstar interpolates all images linearly. The texture coordinates are moved to the center of
    // their texel to get the color of the nearest texel.
    const float4 size(m_image_size.x, m_image_size.y, 1.f, 1.f);
    uv = graph.arithmetic(RPR_MATERIAL_NODE_OP_FLOOR, graph.mul(uv, MaterialValue(size)), MaterialValue(0.f));
    uv = graph.mul(graph.add(uv, MaterialValue(float4(0.5f, 0.5f, 0.f, 0.f))), MaterialValue(float4(1.f) / size));
  }

  return applyTransform(graph, m_output_transform, m_output_offset,
      generateImageTexture(graph, uv, m_wrap_mode_u, m_wrap_mode_v));
}

rpr_image_wrap_type Image::processWrap(const std::string &name)
{
  if (name == "repeat")
    return RPR_IMAGE_WRAP_TYPE_REPEAT;
  if (name == "mirrorRepeat")
    return RPR_IMAGE_WRAP_TYPE_MIRRORED_REPEAT;
  return RPR_IMAGE_WRAP_TYPE_CLAMP_TO_EDGE;
}

} // namespace anari::rpr
