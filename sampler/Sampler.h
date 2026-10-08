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
#include "../Object.h"
#include "util/TransformNode.h"

namespace anari {
namespace rpr {

struct Sampler : public Object
{
 public:
  explicit Sampler(RPRGlobalState *s);
  static Sampler *createInstance(std::string_view type, RPRGlobalState *s);

  void commitParameters() override;

  // samplers are part of material graphs, a changed sampler requires an update of the RPR scene
  void markFinalized() override;

  // adds the nodes of the sampler to a material graph and returns the sampled value
  virtual MaterialValue generateMaterial(MaterialGraph &graph) const = 0;

 protected:
  Attribute m_input_attribute{Attribute::ATTRIBUTE_0};
  mat4      m_output_transform{linalg::identity};
  float4    m_output_offset{0.f};
};

} // namespace rpr

ANARI_TYPEFOR_SPECIALIZATION(rpr::Sampler *, ANARI_SAMPLER);
} // namespace anari
