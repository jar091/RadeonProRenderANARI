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

#include "Sampler.h"
#include "image/Image1D.h"
#include "image/Image2D.h"
#include "image/Image3D.h"
#include "TransformSampler.h"

namespace anari {
namespace rpr {

Sampler::Sampler(RPRGlobalState *s) : Object(ANARI_SAMPLER, s) {}

Sampler *Sampler::createInstance(std::string_view type, RPRGlobalState *s)
{
  if (type == "image1D")
  {
    return new Image1D(s);
  }
  if (type == "image2D")
  {
    return new Image2D(s);
  }
  if (type == "image3D")
  {
    return new Image3D(s);
  }
  if (type == "transform")
  {
    return new TransformSampler(s);
  }
  // the 'primitive' sampler is not supported: RPR materials can not look up the index of a primitive
  return (Sampler *)new UnknownObject(ANARI_SAMPLER, type, s);
}

void Sampler::commitParameters()
{
  const std::string attribute = getParamString("inAttribute", "attribute0");
  m_input_attribute           = attributeFromString(attribute);
  if (m_input_attribute == Attribute::NONE)
  {
    reportMessage(ANARI_SEVERITY_WARNING, "unsupported value '%s' of sampler parameter 'inAttribute'",
        attribute.c_str());
  }
  m_output_transform = getParam<mat4>("outTransform", mat4(linalg::identity));
  m_output_offset    = getParam<float4>("outOffset", float4(0.f));
}

void Sampler::markFinalized()
{
  Object::markFinalized();
  markSceneChanged();
}

} // namespace rpr

ANARI_TYPEFOR_DEFINITION(rpr::Sampler *);

} // namespace anari
