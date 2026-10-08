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

#include "Image2D.h"

namespace anari::rpr {

Image2D::Image2D(RPRGlobalState *s) : Image(s), m_data(this) {}

void Image2D::commitParameters()
{
  Image::commitParameters();
  m_wrap_mode_u = processWrap(getParamString("wrapMode1", "clampToEdge"));
  m_wrap_mode_v = processWrap(getParamString("wrapMode2", "clampToEdge"));
  m_data        = getArray2D(this, "image");
}

void Image2D::finalize()
{
  if (!m_data)
  {
    reportMessage(ANARI_SEVERITY_WARNING, "missing required parameter 'image' on image2D sampler");
    clearImage();
    return;
  }

  setImage(m_data->data(), m_data->elementType(), m_data->size(0), m_data->size(1));
}

} // namespace anari::rpr
