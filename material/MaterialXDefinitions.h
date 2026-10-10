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

#include <cstddef>

namespace anari::rpr {

// MaterialX document with node definitions
struct MaterialXDefinition
{
  const char *name;
  const char *data; // not terminated by a zero
  size_t      size;
};

// The node definitions which are part of the device: the MaterialX loader of RPR has no definitions of
// the surface shaders (open_pbr_surface, standard_surface, UsdPreviewSurface) built in, they are given
// to it as dependencies of every document. The documents are the files in material/materialx, which
// CMake compiles into the device (MaterialXDefinitions.cpp.in), so no file is needed at runtime.
extern const MaterialXDefinition MATERIALX_DEFINITIONS[];
extern const size_t              NUM_MATERIALX_DEFINITIONS;

} // namespace anari::rpr
