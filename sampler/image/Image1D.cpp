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

#include "Image1D.h"

namespace anari::rpr {

Image1D::Image1D(RPRGlobalState *s) : Image(s), m_data(this) {}

void Image1D::commitParameters()
{
  Image::commitParameters();
  m_wrap_mode_u = processWrap(getParamString("wrapMode", "clampToEdge"));
  m_wrap_mode_v = RPR_IMAGE_WRAP_TYPE_CLAMP_TO_EDGE;
  m_data        = getArray1D(this, "image");
}

void Image1D::finalize()
{
  if (!m_data)
  {
    reportMessage(ANARI_SEVERITY_WARNING, "missing required parameter 'image' on image1D sampler");
    clearImage();
    return;
  }

  // an image with a height of one pixel, the second texture coordinate does not matter
  setImage(m_data->begin(), m_data->elementType(), m_data->size(), 1);
}

} // namespace anari::rpr
