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

#include "MaterialGraph.h"

namespace anari::rpr {

MaterialGraph::MaterialGraph(RPRGlobalState *state, const AttributeContext &context)
    : m_state(state), m_context(context)
{}

MaterialGraph::~MaterialGraph()
{
  for (rpr_material_node node : m_nodes)
    m_state->deleteLater(node, DeleteOrder::MATERIAL);
}

rpr_material_node MaterialGraph::createNode(rpr_material_node_type type)
{
  rpr_material_node node = nullptr;
  CHECK(rprMaterialSystemCreateNode(m_state->matsys, type, &node))
  if (node)
    m_nodes.push_back(node);
  return node;
}

void MaterialGraph::setInput(rpr_material_node node, rpr_material_node_input input, const MaterialValue &value)
{
  if (!node)
    return;
  if (value.isNode())
  {
    CHECK(rprMaterialNodeSetInputNByKey(node, input, value.node))
  } else
  {
    CHECK(rprMaterialNodeSetInputFByKey(
        node, input, value.constant.x, value.constant.y, value.constant.z, value.constant.w))
  }
}

void MaterialGraph::setInputU(rpr_material_node node, rpr_material_node_input input, rpr_uint value)
{
  if (!node)
    return;
  CHECK(rprMaterialNodeSetInputUByKey(node, input, value))
}

MaterialValue MaterialGraph::arithmetic(
    rpr_material_node_arithmetic_operation op, const MaterialValue &a, const MaterialValue &b)
{
  if (!a.isNode() && !b.isNode())
  {
    switch (op)
    {
    case RPR_MATERIAL_NODE_OP_ADD:
      return MaterialValue(a.constant + b.constant);
    case RPR_MATERIAL_NODE_OP_SUB:
      return MaterialValue(a.constant - b.constant);
    case RPR_MATERIAL_NODE_OP_MUL:
      return MaterialValue(a.constant * b.constant);
    case RPR_MATERIAL_NODE_OP_MIN:
      return MaterialValue(linalg::min(a.constant, b.constant));
    case RPR_MATERIAL_NODE_OP_MAX:
      return MaterialValue(linalg::max(a.constant, b.constant));
    case RPR_MATERIAL_NODE_OP_DOT4: {
      return MaterialValue(linalg::dot(a.constant, b.constant));
    }
    case RPR_MATERIAL_NODE_OP_FLOOR:
      return MaterialValue(linalg::floor(a.constant));
    case RPR_MATERIAL_NODE_OP_GREATER_OR_EQUAL:
      return MaterialValue(float4(a.constant.x >= b.constant.x ? 1.f : 0.f, a.constant.y >= b.constant.y ? 1.f : 0.f,
          a.constant.z >= b.constant.z ? 1.f : 0.f, a.constant.w >= b.constant.w ? 1.f : 0.f));
    default:
      break;
    }
  }

  rpr_material_node node = createNode(RPR_MATERIAL_NODE_ARITHMETIC);
  setInputU(node, RPR_MATERIAL_INPUT_OP, op);
  setInput(node, RPR_MATERIAL_INPUT_COLOR0, a);
  setInput(node, RPR_MATERIAL_INPUT_COLOR1, b);
  return MaterialValue(node);
}

MaterialValue MaterialGraph::add(const MaterialValue &a, const MaterialValue &b)
{
  return arithmetic(RPR_MATERIAL_NODE_OP_ADD, a, b);
}

MaterialValue MaterialGraph::sub(const MaterialValue &a, const MaterialValue &b)
{
  return arithmetic(RPR_MATERIAL_NODE_OP_SUB, a, b);
}

MaterialValue MaterialGraph::mul(const MaterialValue &a, const MaterialValue &b)
{
  return arithmetic(RPR_MATERIAL_NODE_OP_MUL, a, b);
}

MaterialValue MaterialGraph::select(const MaterialValue &v, int component)
{
  if (!v.isNode())
    return MaterialValue(v.constant[component]);

  static const rpr_material_node_arithmetic_operation ops[4] = {RPR_MATERIAL_NODE_OP_SELECT_X,
      RPR_MATERIAL_NODE_OP_SELECT_Y, RPR_MATERIAL_NODE_OP_SELECT_Z, RPR_MATERIAL_NODE_OP_SELECT_W};
  rpr_material_node node = createNode(RPR_MATERIAL_NODE_ARITHMETIC);
  setInputU(node, RPR_MATERIAL_INPUT_OP, ops[component]);
  setInput(node, RPR_MATERIAL_INPUT_COLOR0, v);
  return MaterialValue(node);
}

MaterialValue MaterialGraph::attribute(Attribute attribute)
{
  switch (attribute)
  {
  case Attribute::ATTRIBUTE_0:
  case Attribute::ATTRIBUTE_1:
  case Attribute::ATTRIBUTE_2:
  case Attribute::ATTRIBUTE_3:
  case Attribute::COLOR: {
    const int slot = int(attribute);
    if (m_context.constant[slot].has_value())
      return MaterialValue(*m_context.constant[slot]);
    if (m_context.buffer[slot])
    {
      // one value per instance of the shape: the object ID is the index of the value in the buffer
      rpr_material_node lookup = createNode(RPR_MATERIAL_NODE_INPUT_LOOKUP);
      setInputU(lookup, RPR_MATERIAL_INPUT_VALUE, RPR_MATERIAL_NODE_LOOKUP_OBJECT_ID);
      rpr_material_node sampler = createNode(RPR_MATERIAL_NODE_BUFFER_SAMPLER);
      setInput(sampler, RPR_MATERIAL_INPUT_UV, MaterialValue(lookup));
      if (sampler)
      {
        CHECK(rprMaterialNodeSetInputBufferDataByKey(sampler, RPR_MATERIAL_INPUT_DATA, m_context.buffer[slot]))
      }
      return MaterialValue(sampler);
    }
    // the primvar key of an attribute is its slot
    usesPrimvars           = true;
    rpr_material_node node = createNode(RPR_MATERIAL_NODE_PRIMVAR_LOOKUP);
    setInputU(node, RPR_MATERIAL_INPUT_VALUE, rpr_uint(slot));
    return MaterialValue(node);
  }
  case Attribute::WORLD_POSITION:
  case Attribute::OBJECT_POSITION:
  case Attribute::WORLD_NORMAL:
  case Attribute::OBJECT_NORMAL: {
    const bool        isPosition = attribute == Attribute::WORLD_POSITION || attribute == Attribute::OBJECT_POSITION;
    rpr_material_node node       = createNode(RPR_MATERIAL_NODE_INPUT_LOOKUP);
    setInputU(node, RPR_MATERIAL_INPUT_VALUE,
        attribute == Attribute::WORLD_POSITION ? RPR_MATERIAL_NODE_LOOKUP_P
            : attribute == Attribute::OBJECT_POSITION ? RPR_MATERIAL_NODE_LOOKUP_P_LOCAL
                                                      : RPR_MATERIAL_NODE_LOOKUP_N);
    // the fourth component is 1 for positions and 0 for directions
    MaterialValue xyz = mul(MaterialValue(node), MaterialValue(float4(1.f, 1.f, 1.f, 0.f)));
    return isPosition ? add(xyz, MaterialValue(float4(0.f, 0.f, 0.f, 1.f))) : xyz;
  }
  default:
    break;
  }
  return MaterialValue(DEFAULT_ATTRIBUTE_VALUE);
}

const AttributeContext &MaterialGraph::context() const
{
  return m_context;
}

RPRGlobalState *MaterialGraph::deviceState() const
{
  return m_state;
}

} // namespace anari::rpr
