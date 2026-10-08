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

#include "../../../Array.h"

namespace anari::rpr {

// Names of the attribute slots: "attribute0".."attribute3" and "color"
const char *attributeName(int slot);

// Describes where a material finds the attributes of the geometry it is applied on.
//
// The attributes which a geometry defines per vertex or per primitive are stored as primvars of the RPR
// mesh, the key of a primvar is the attribute slot. Geometries which are a set of instances of one RPR
// mesh (spheres) have one value per instance instead: the values are stored in a RPR buffer and the
// object ID of an instance is its index in the buffer. All other attributes are constant for the whole
// shape: either a uniform value of the geometry or of the instance, or the default value (0, 0, 0, 1).
// A material graph is built for one attribute context, shapes with equal contexts share the graph.
struct AttributeContext
{
  // constant value of the attribute, without value the attribute is read from the buffer or, if there is
  // no buffer, from the primvar of the shape
  UniformAttributeSet constant;
  // values of the attribute per object ID
  std::array<rpr_buffer, NUM_ATTRIBUTES> buffer{};

  bool operator<(const AttributeContext &other) const;
};

} // namespace anari::rpr
