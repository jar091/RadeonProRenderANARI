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

#include "MaterialXMaterial.h"
#include "MaterialXDefinitions.h"
#include "MaterialXDocument.h"
// rpr
#include "RadeonProRender_MaterialX.h"
// std
#include <algorithm>
#include <chrono>
#include <fstream>
#include <sstream>
#include <string_view>

namespace anari::rpr {

namespace {

// color of the diffuse material which replaces a document that can not be loaded
constexpr float FALLBACK_COLOR = 0.8f;

std::string readTextFile(const std::string &path)
{
  std::ifstream file(path, std::ios::binary);
  if (!file)
    return "";
  std::stringstream text;
  text << file.rdbuf();
  return text.str();
}

// RPR calls ////////////////////////////////////////////////////////////////////

// Things which are the same for all documents of a context. They are set when the first document is
// loaded.
void initMaterialX(RPRGlobalState *state)
{
  if (state->materialXInitialized)
    return;
  state->materialXInitialized = true;

  auto deviceState = [&]() { return state; };

  // The node definitions of the surface shaders are not built into the loader. The list of the
  // dependencies is kept by the context, the loader parses them again together with every document
  // (about 1.5 ms per definition).
  for (size_t i = 0; i < NUM_MATERIALX_DEFINITIONS; i++)
  {
    const MaterialXDefinition &definition = MATERIALX_DEFINITIONS[i];
    CHECK(rprMaterialXAddDependencyMtlxAsBuffer(state->context, definition.data, definition.size))
  }

  // geompropvalue nodes read the attributes of the geometry by their ANARI names: the attributes are
  // primvars of the meshes, the key of a primvar is the attribute slot
  for (int slot = 0; slot < NUM_ATTRIBUTES; slot++)
  {
    CHECK(rprMaterialXBindGeomPropToPrimvar(state->context, attributeName(slot), rpr_uint(slot)))
  }
}

// warnings and errors of the last document, one per line
std::string loaderMessages(rpr_context context)
{
  size_t size = 0;
  if (rprMaterialXGetLoaderMessages(context, 0, nullptr, &size) != RPR_SUCCESS || size == 0)
    return "";
  std::string text(size, '\0');
  if (rprMaterialXGetLoaderMessages(context, size, text.data(), nullptr) != RPR_SUCCESS)
    return "";
  // without the terminating zero and trailing line breaks
  while (!text.empty() && (text.back() == '\0' || text.back() == '\n' || text.back() == '\r' || text.back() == ' '))
    text.pop_back();
  return text;
}

// A document with a node the loader does not know is loaded without error, but the node has no
// output: the material is white, black or has wrong colors. The loader reports such a node as
// "unkonwn node = <name>".
bool hasUnknownNode(const std::string &messages)
{
  return messages.find("unkonwn node") != std::string::npos || messages.find("unknown node") != std::string::npos;
}

} // namespace

// MaterialXMaterial definitions ////////////////////////////////////////////////

MaterialXMaterial::MaterialXMaterial(RPRGlobalState *s) : Material(s) {}

MaterialXMaterial::~MaterialXMaterial()
{
  clear();
}

void MaterialXMaterial::commitParameters()
{
  Material::commitParameters();

  m_source        = getParamString("source", "");
  m_source_type   = getParamString("sourceType", "documentInline");
  m_material_name = getParamString("materialName", "");
  if (m_source_type != "documentInline" && m_source_type != "documentFile")
  {
    reportMessage(ANARI_SEVERITY_WARNING,
        "unknown value '%s' of materialx parameter 'sourceType' (documentInline, documentFile), the source is "
        "used as document",
        m_source_type.c_str());
    m_source_type = "documentInline";
  }

  // The images of the document are the sampler parameters which have the name of a file of the
  // document. The names are taken from the document: the parameters of the object can not be listed
  // here without a race with the application, which is allowed to set parameters at any time.
  m_images.clear();
  const std::string document = m_source_type == "documentFile" ? readTextFile(m_source) : m_source;
  for (const std::string &name : materialXImageFileNames(document))
  {
    if (!hasParam(name))
      continue; // a file on disk or a missing image, which the loader reports
    auto *sampler = dynamic_cast<Image2D *>(getParamObject<helium::BaseObject>(name));
    if (!sampler)
    {
      reportMessage(ANARI_SEVERITY_WARNING,
          "parameter '%s' of materialx material is not an image2D sampler, the image is missing", name.c_str());
      continue;
    }
    m_images.push_back({name, helium::ChangeObserverPtr<Image2D>(this, sampler)});
  }
}

void MaterialXMaterial::finalize()
{
  ImageList images;
  for (const ImageParameter &parameter : m_images)
  {
    if (parameter.sampler && parameter.sampler->isValid())
    {
      images.emplace_back(parameter.name, parameter.sampler->image());
    } else
    {
      reportMessage(ANARI_SEVERITY_WARNING, "sampler '%s' of materialx material has no image",
          parameter.name.c_str());
    }
  }

  // nothing to do if the document and its images are the ones which were loaded the last time
  m_reloaded = !m_load_attempted || m_source != m_loaded_source || m_source_type != m_loaded_source_type
      || m_material_name != m_loaded_material_name || images != m_loaded_images;
  if (!m_reloaded)
    return;

  Material::finalize();
  clear();

  m_load_attempted       = true;
  m_loaded_source        = m_source;
  m_loaded_source_type   = m_source_type;
  m_loaded_material_name = m_material_name;
  m_loaded_images        = images;

  if (!load(images))
    clear();
}

void MaterialXMaterial::markFinalized()
{
  // the RPR scene is not updated if the material is still the same
  if (m_reloaded)
    Material::markFinalized();
  else
    Object::markFinalized();
}

bool MaterialXMaterial::getProperty(
    const std::string_view &name, ANARIDataType type, void *ptr, uint64_t size, uint32_t flags)
{
  if (name == "valid" && type == ANARI_BOOL)
  {
    helium::writeToVoidP(ptr, m_node != nullptr);
    return true;
  }
  return Material::getProperty(name, type, ptr, size, flags);
}

bool MaterialXMaterial::load(const ImageList &images)
{
  auto *state = deviceState();
  if (!state->context || !state->matsys || state->contextFailed)
    return false;

  if (m_source.empty())
  {
    reportMessage(ANARI_SEVERITY_WARNING,
        "missing required parameter 'source' on materialx material, a diffuse material is used instead");
    return false;
  }

  const auto start = std::chrono::steady_clock::now();
  const bool isFile = m_source_type == "documentFile";

  initMaterialX(state);

  // A file is loaded by RPR, which resolves its includes and the paths of its images. It is used as
  // it is.
  std::string document;
  if (!isFile)
  {
    std::vector<std::string> warnings;
    document = prepareMaterialXDocument(m_source, m_material_name, warnings);
    for (const std::string &warning : warnings)
      reportMessage(ANARI_SEVERITY_WARNING, "materialx: %s", warning.c_str());
  }

  // texcoord nodes with the index 0 read the texture coordinates of the mesh, all other attributes are
  // read from primvars by geompropvalue nodes
  m_uses_primvars = (isFile ? readTextFile(m_source) : document).find("<geompropvalue") != std::string::npos;

  // The loader of RPR always fills a new node: loading another document into a node which is in use
  // does not change the material.
  CHECK(rprMaterialSystemCreateNode(state->matsys, RPR_MATERIAL_NODE_MATX, &m_node))
  if (!m_node)
    return false;

  // The images come from the samplers, nothing is read from disk for them. The list of the images
  // is kept by the context: it is only valid while this document is loaded.
  CHECK(rprMaterialXCleanPreloadedImages(state->context))
  for (const auto &image : images)
  {
    CHECK(rprMaterialXAddPreloadedImage(state->context, image.first.c_str(), image.second))
  }

  // the loader reports a document which can not be parsed with a C++ exception
  rpr_status status    = RPR_ERROR_INTERNAL_ERROR;
  bool       exception = false;
  try
  {
    status = isFile ? rprMaterialXSetFile(m_node, m_source.c_str())
                    : rprMaterialXSetFileAsBuffer(m_node, document.data(), document.size());
  } catch (...)
  {
    exception = true;
  }
  const std::string messages = loaderMessages(state->context);

  CHECK(rprMaterialXCleanPreloadedImages(state->context))

  if (exception || status != RPR_SUCCESS)
  {
    reportMessage(ANARI_SEVERITY_ERROR,
        "the MaterialX document can not be loaded (%s %d), a diffuse material is used instead%s%s",
        exception ? "exception of the loader, RPR error" : "RPR error", int(status),
        messages.empty() ? "" : ":\n", messages.c_str());
    return false;
  }
  if (hasUnknownNode(messages))
  {
    reportMessage(ANARI_SEVERITY_ERROR,
        "the MaterialX document uses nodes which RadeonProRender does not support, a diffuse material is used "
        "instead:\n%s",
        messages.c_str());
    return false;
  }
  if (!messages.empty())
    reportMessage(ANARI_SEVERITY_WARNING, "messages of the MaterialX loader:\n%s", messages.c_str());

  reportMessage(ANARI_SEVERITY_DEBUG, "loaded MaterialX document (%zu bytes, %zu images) in %.2f ms",
      m_source.size(), images.size(),
      std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - start).count());
  return true;
}

void MaterialXMaterial::clear()
{
  deviceState()->deleteLater(m_node, DeleteOrder::MATERIAL);
  m_node = nullptr;
}

void MaterialXMaterial::generateMaterial(MaterialGraph &graph) const
{
  if (m_node)
  {
    // The node does not depend on the attribute context: the document reads the attributes from the
    // primvars of the mesh. It is owned by the material, not by the graph.
    graph.root         = m_node;
    graph.usesPrimvars = m_uses_primvars;
    return;
  }

  rpr_material_node fallback = graph.createNode(RPR_MATERIAL_NODE_DIFFUSE);
  graph.setInput(fallback, RPR_MATERIAL_INPUT_COLOR, MaterialValue(float4(float3(FALLBACK_COLOR), 1.f)));
  graph.root = fallback;
}

} // namespace anari::rpr
