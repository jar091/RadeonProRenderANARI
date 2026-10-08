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

#include "TransformNode.h"

namespace anari::rpr {

MaterialValue applyTransform(
    MaterialGraph &graph, const mat4 &transform, const float4 &offset, const MaterialValue &value)
{
  MaterialValue result = value;

  if (!value.isNode())
  {
    result = MaterialValue(linalg::mul(transform, value.constant));
  } else if (!isIdentity(transform))
  {
    // Every component of the result is the dot product of the value and a row of the matrix. The dot
    // product is in all components of the output of the node, the mask selects the one of the row.
    for (int row = 0; row < 4; row++)
    {
      const float4 matrixRow(transform[0][row], transform[1][row], transform[2][row], transform[3][row]);
      float4       mask(0.f);
      mask[row] = 1.f;
      const MaterialValue component = graph.mul(
          graph.arithmetic(RPR_MATERIAL_NODE_OP_DOT4, value, MaterialValue(matrixRow)), MaterialValue(mask));
      result = row == 0 ? component : graph.add(result, component);
    }
  }

  if (offset != float4(0.f))
    result = graph.add(result, MaterialValue(offset));

  return result;
}

} // namespace anari::rpr
