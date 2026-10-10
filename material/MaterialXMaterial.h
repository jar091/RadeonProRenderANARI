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

#include "../sampler/image/Image2D.h"
#include "Material.h"
// std
#include <string>
#include <utility>
#include <vector>

namespace anari::rpr {

// Material which is defined by a MaterialX document. The document is loaded by the MaterialX loader of
// RPR into a single node (RPR_MATERIAL_NODE_MATX).
//
// Parameters:
//   source        the document: XML text or, with the source type 'documentFile', the path of a file
//   sourceType    'documentInline' (default) or 'documentFile'
//   materialName  name of the surfacematerial to use, the first one of the document by default
//   <file name>   image2D sampler: the image of every 'filename' input of the document whose value is
//                 the name of the parameter (for example "image1/file"). Only the image of the
//                 sampler is used, the document has its own texture coordinates and address modes.
//
// The node definitions of open_pbr_surface, standard_surface and UsdPreviewSurface are part of the
// device (see MaterialXDefinitions.h), other definitions have to be part of the document.
// Geometry attributes: the texture coordinates with the index 0 are 'attribute0', the texture
// coordinates with the index N are 'attributeN' and the geometry color is 'color'. A 'geompropvalue'
// node can read "attribute0".."attribute3" and "color" by name.
// An inline document is adapted to the loader before it is loaded, see MaterialXDocument.h.
// A document which can not be loaded is reported as an error and replaced by a diffuse material.
struct MaterialXMaterial : public Material
{
  explicit MaterialXMaterial(RPRGlobalState *s);
  ~MaterialXMaterial() override;

  void commitParameters() override;
  void finalize() override;
  void markFinalized() override;

  // "valid" is false if the document could not be loaded and the fallback material is used
  bool getProperty(
      const std::string_view &name, ANARIDataType type, void *ptr, uint64_t size, uint32_t flags) override;

 protected:
  void generateMaterial(MaterialGraph &graph) const override;

 private:
  struct ImageParameter
  {
    std::string                        name;
    helium::ChangeObserverPtr<Image2D> sampler;
  };
  using ImageList = std::vector<std::pair<std::string, rpr_image>>;

  // creates the node from the document, returns false if the document can not be used
  bool load(const ImageList &images);
  void clear();

  std::string                 m_source;
  std::string                 m_source_type;
  std::string                 m_material_name;
  std::vector<ImageParameter> m_images;

  rpr_material_node m_node{nullptr};
  bool              m_uses_primvars{false}; // the document has geompropvalue nodes
  // The document is loaded again only if one of these values is different from the last time:
  // loading a document restarts the rendering and is too slow to be done for nothing.
  bool        m_load_attempted{false};
  std::string m_loaded_source;
  std::string m_loaded_source_type;
  std::string m_loaded_material_name;
  ImageList   m_loaded_images;
  bool        m_reloaded{false}; // by the last finalize()
};

} // namespace anari::rpr
