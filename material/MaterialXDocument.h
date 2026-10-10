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

#include <string>
#include <string_view>
#include <vector>

// Functions which read and rewrite the text of a MaterialX document. They do not use RPR or ANARI.

namespace anari::rpr {

// Values of all inputs of the type 'filename' of a document: the names of its images
std::vector<std::string> materialXImageFileNames(std::string_view document);

// Adapts a document to the MaterialX loader of RPR (Northstar). The loader takes a document as it is,
// but it renders some valid constructs wrong without any message. These are rewritten into equivalent
// constructs which the loader renders right:
//
// - A document-level <output> element is taken as the result of the document: the material is black.
//   Inputs which are connected to such an output (output="name" without node name, as Blender writes
//   them) are connected to the node of the output instead and the outputs are removed.
// - The last <surfacematerial> of a document is used. All of them are removed except the one with the
//   given name or, without name, the first one.
// - The index of <texcoord> nodes is ignored, all of them read the texture coordinates of the mesh
//   ('attribute0'). A texcoord node with the index N > 0 becomes a <geompropvalue> node which reads
//   "attributeN". <geomcolor> nodes are unknown, they become <geompropvalue> nodes which read "color".
// - <separate2>, <separate3> and <separate4> nodes are unknown, they become <extract> nodes.
// - <mix> and <add> of two EDFs give wrong results (mix: fg + mix * bg, add: the first operand only).
//   They become one <uniform_edf> whose color is the mix or the sum of the colors of the operands.
// - The BSDF nodes square their 'roughness' input, which already is the squared roughness in MaterialX,
//   unless it comes from a <roughness_anisotropy> node. Other roughness inputs get their square root.
// - The opacity of a <surface> node is ignored. The BSDF of the surface is mixed with a lobe which
//   transmits the light straight through and its EDF is multiplied by the opacity instead.
//
// Only the nodes at document level are rewritten, node graphs are left as they are. What can not be
// rewritten is described in 'warnings'.
std::string prepareMaterialXDocument(
    const std::string &source, const std::string &materialName, std::vector<std::string> &warnings);

} // namespace anari::rpr
