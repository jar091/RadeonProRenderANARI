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

#include "TransformSampler.h"

namespace anari::rpr {

TransformSampler::TransformSampler(RPRGlobalState *s) : Sampler(s) {}

void TransformSampler::commitParameters()
{
  Sampler::commitParameters();
  // KHR_SAMPLER_TRANSFORM names the matrix 'outTransform', older applications use 'transform'
  m_output_transform = getParam<mat4>("outTransform", getParam<mat4>("transform", mat4(linalg::identity)));
}

MaterialValue TransformSampler::generateMaterial(MaterialGraph &graph) const
{
  return applyTransform(graph, m_output_transform, m_output_offset, graph.attribute(m_input_attribute));
}

} // namespace anari::rpr
