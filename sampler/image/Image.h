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

#pragma once
#include "../Sampler.h"

namespace anari::rpr {

// Creates a RPR image with four components from 'width' x 'height' elements of an ANARI array.
// The first row of the RPR image is the last row of the ANARI array if 'flipRows' is set.
rpr_image createImageFromData(const RPRGlobalState *state, const void *data, ANARIDataType elementType,
    size_t width, size_t height, bool flipRows);

struct Image : public Sampler
{
 public:
  explicit Image(RPRGlobalState *s);
  ~Image() override;

  void commitParameters() override;
  bool isValid() const override;

  MaterialValue generateMaterial(MaterialGraph &graph) const override;

  // the RPR image of the sampler, null if it is not valid. It is replaced when the sampler is finalized.
  rpr_image image() const;

 protected:
  static rpr_image_wrap_type processWrap(const std::string &name);

  // replaces the RPR image by a new one which is created from the data of an array
  void setImage(const void *data, ANARIDataType elementType, size_t width, size_t height);
  void clearImage();

  // adds the node which samples the RPR image at texture coordinates
  MaterialValue generateImageTexture(MaterialGraph &graph, const MaterialValue &uv,
      rpr_image_wrap_type wrapU, rpr_image_wrap_type wrapV) const;

  mat4                  m_input_transform{linalg::identity};
  float4                m_input_offset{0.f};
  rpr_image             m_image{};
  float2                m_image_size{1.f, 1.f};
  rpr_image_filter_type m_filter{RPR_IMAGE_FILTER_TYPE_LINEAR};
  rpr_image_wrap_type   m_wrap_mode_u{RPR_IMAGE_WRAP_TYPE_CLAMP_TO_EDGE};
  rpr_image_wrap_type   m_wrap_mode_v{RPR_IMAGE_WRAP_TYPE_CLAMP_TO_EDGE};
};

} // namespace anari::rpr
