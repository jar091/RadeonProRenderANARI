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

#include "../scene/geometry/attributes/Attribute.h"
// helium
#include "helium/utility/TimeStamp.h"
// std
#include <vector>

namespace anari::rpr {

// Value of an input of a material node: a constant or the output of another node
struct MaterialValue
{
  MaterialValue() = default;
  MaterialValue(const float4 &c) : constant(c) {}
  MaterialValue(float c) : constant(c, c, c, c) {}
  MaterialValue(rpr_material_node n) : node(n) {}

  bool isNode() const
  {
    return node != nullptr;
  }

  rpr_material_node node{nullptr};
  float4            constant{0.f, 0.f, 0.f, 1.f};
};

// Set of RPR material nodes which implement an ANARI material for shapes with a given attribute context.
// The graph owns its nodes.
struct MaterialGraph
{
  MaterialGraph(RPRGlobalState *state, const AttributeContext &context);
  ~MaterialGraph();

  MaterialGraph(const MaterialGraph &)            = delete;
  MaterialGraph &operator=(const MaterialGraph &) = delete;

  rpr_material_node createNode(rpr_material_node_type type);

  void setInput(rpr_material_node node, rpr_material_node_input input, const MaterialValue &value);
  void setInputU(rpr_material_node node, rpr_material_node_input input, rpr_uint value);

  // arithmetic node, constant operands are folded
  MaterialValue arithmetic(rpr_material_node_arithmetic_operation op, const MaterialValue &a, const MaterialValue &b);
  MaterialValue add(const MaterialValue &a, const MaterialValue &b);
  MaterialValue sub(const MaterialValue &a, const MaterialValue &b);
  MaterialValue mul(const MaterialValue &a, const MaterialValue &b);
  // (v[component], v[component], v[component], v[component])
  MaterialValue select(const MaterialValue &v, int component);

  // value of a geometry attribute ("attribute0", "color", "worldPosition", ...)
  MaterialValue attribute(Attribute attribute);

  const AttributeContext &context() const;
  RPRGlobalState         *deviceState() const;

  // the node which is assigned to the shapes
  rpr_material_node root{nullptr};
  // The material reads attributes of the geometry which are primvars of its mesh.
  bool usesPrimvars{false};
  // Identifies the graph. The address of the root node does not: a new node can get the address of a
  // node which was deleted.
  const helium::TimeStamp id{helium::newTimeStamp()};

 private:
  RPRGlobalState                *m_state{nullptr};
  AttributeContext               m_context;
  std::vector<rpr_material_node> m_nodes;
};

} // namespace anari::rpr
