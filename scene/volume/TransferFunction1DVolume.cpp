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

#include "TransferFunction1DVolume.h"
// std
#include <vector>

namespace anari::rpr {

TransferFunction1DVolume::TransferFunction1DVolume(RPRGlobalState *s)
    : Volume(s), m_color_data(this), m_opacity_data(this)
{}

void TransferFunction1DVolume::commitParameters()
{
  // 'value' is the parameter of ANARI 1.0, 'field' was used by older versions
  m_field = getObjectParam<SpatialField>(this, "value", ANARI_SPATIAL_FIELD);
  if (!m_field)
    m_field = getObjectParam<SpatialField>(this, "field", ANARI_SPATIAL_FIELD);

  m_color_data   = getArray1D(this, "color");
  m_opacity_data = getArray1D(this, "opacity");

  m_color = getParam<float4>("color", float4(1.f, 1.f, 1.f, 1.f));
  float3 color3(1.f);
  if (getParam("color", ANARI_FLOAT32_VEC3, &color3))
    m_color = float4(color3, 1.f);
  m_opacity = getParam<float>("opacity", 1.f);

  m_value_range = getParam<box1>("valueRange", box1(0.f, 1.f));
  float2 range(0.f, 1.f);
  if (getParam("valueRange", ANARI_FLOAT32_VEC2, &range))
    m_value_range = box1(range.x, range.y);

  m_unit_distance = getParam<float>("unitDistance", 1.f);
  if (!(m_unit_distance > 0.f))
    m_unit_distance = 1.f;
}

bool TransferFunction1DVolume::isValid() const
{
  return m_volume_material != nullptr;
}

box3 TransferFunction1DVolume::bounds() const
{
  return m_field && m_field->isValid() ? m_field->bounds() : emptyBox();
}

void TransferFunction1DVolume::finalize()
{
  clear();

  if (!m_field || !m_field->isValid())
  {
    reportMessage(ANARI_SEVERITY_WARNING, "transferFunction1D volume has no supported spatial field 'value'");
    return;
  }

  auto *state = deviceState();

  // Coarse fields are resampled to a grid with about 128 samples along the longest axis, because RPR
  // does not interpolate between the voxels of a grid.
  const uint32_t           factor = SpatialField::resamplingFactor(m_field->size(), 128);
  uint3                    size(0u);
  const std::vector<float> values  = m_field->resample(factor, size);
  const float3             spacing = m_field->spacing() / float(factor);

  const size_t numVoxels  = values.size();
  const float  rangeSize  = m_value_range.upper - m_value_range.lower;
  const float  rangeScale = rangeSize != 0.f ? 1.f / rangeSize : 0.f;

  // normalize values of grid and calculate density grid values, voxels without density are left out
  std::vector<uint32_t> indices;
  std::vector<float>    normalizedGridData;
  std::vector<float>    densityGridData;
  indices.reserve(numVoxels * 3);
  normalizedGridData.reserve(numVoxels);
  densityGridData.reserve(numVoxels);

  size_t index = 0;
  for (uint32_t z = 0; z < size.z; z++)
  {
    for (uint32_t y = 0; y < size.y; y++)
    {
      for (uint32_t x = 0; x < size.x; x++, index++)
      {
        const float voxelValue      = values[index];
        const float normalizedValue = std::clamp((voxelValue - m_value_range.lower) * rangeScale, 0.f, 1.f);
        const float density         = processDensity(normalizedValue);
        if (!(density > 0.f))
          continue;
        indices.push_back(x);
        indices.push_back(y);
        indices.push_back(z);
        normalizedGridData.push_back(normalizedValue);
        densityGridData.push_back(density);
      }
    }
  }

  if (densityGridData.empty())
  {
    reportMessage(ANARI_SEVERITY_DEBUG, "transferFunction1D volume is completely transparent");
    return;
  }

  CHECK(rprContextCreateGrid(state->context, &m_color_grid, size.x, size.y, size.z, indices.data(),
      indices.size() / 3, RPR_GRID_INDICES_TOPOLOGY_XYZ_U32, normalizedGridData.data(),
      sizeof(float) * normalizedGridData.size(), 0))
  CHECK(rprContextCreateGrid(state->context, &m_density_grid, size.x, size.y, size.z, indices.data(),
      indices.size() / 3, RPR_GRID_INDICES_TOPOLOGY_XYZ_U32, densityGridData.data(),
      sizeof(float) * densityGridData.size(), 0))
  if (!m_color_grid || !m_density_grid)
  {
    clear();
    return;
  }

  CHECK(rprMaterialSystemCreateNode(state->matsys, RPR_MATERIAL_NODE_GRID_SAMPLER, &m_color_grid_material))
  CHECK(rprMaterialNodeSetInputGridDataByKey(m_color_grid_material, RPR_MATERIAL_INPUT_DATA, m_color_grid))
  CHECK(rprMaterialSystemCreateNode(state->matsys, RPR_MATERIAL_NODE_GRID_SAMPLER, &m_density_grid_material))
  CHECK(rprMaterialNodeSetInputGridDataByKey(m_density_grid_material, RPR_MATERIAL_INPUT_DATA, m_density_grid))

  // the colors of the transfer function are a lookup table for the normalized values of the field
  std::vector<float4> colors;
  if (m_color_data && m_color_data->size() > 0)
  {
    colors.resize(m_color_data->size());
    for (size_t i = 0; i < colors.size(); i++)
      colors[i] = readAttributeValue(m_color_data.get(), i);
  } else
  {
    colors.push_back(m_color);
  }
  rpr_image_desc imageDesc;
  imageDesc.image_width       = rpr_uint(colors.size());
  imageDesc.image_height      = 1;
  imageDesc.image_depth       = 0;
  imageDesc.image_row_pitch   = rpr_uint(colors.size() * sizeof(float4));
  imageDesc.image_slice_pitch = 0;
  CHECK(rprContextCreateImage(
      state->context, {4, RPR_COMPONENT_TYPE_FLOAT32}, &imageDesc, colors.data(), &m_color_map))
  CHECK(rprMaterialSystemCreateNode(state->matsys, RPR_MATERIAL_NODE_IMAGE_TEXTURE, &m_color_material))
  CHECK(rprMaterialSystemCreateNode(state->matsys, RPR_MATERIAL_NODE_VOLUME, &m_volume_material))
  if (!m_color_map || !m_color_material || !m_volume_material)
  {
    clear();
    return;
  }
  CHECK(rprImageSetGamma(m_color_map, 1.f))
  CHECK(rprMaterialNodeSetInputImageDataByKey(m_color_material, RPR_MATERIAL_INPUT_DATA, m_color_map))
  CHECK(rprMaterialNodeSetInputNByKey(m_color_material, RPR_MATERIAL_INPUT_UV, m_color_grid_material))
  CHECK(rprMaterialNodeSetInputUByKey(
      m_color_material, RPR_MATERIAL_INPUT_WRAP_U, RPR_IMAGE_WRAP_TYPE_CLAMP_TO_EDGE))
  CHECK(rprMaterialNodeSetInputUByKey(
      m_color_material, RPR_MATERIAL_INPUT_WRAP_V, RPR_IMAGE_WRAP_TYPE_CLAMP_TO_EDGE))

  // the opacity of the transfer function is the extinction of the volume per unit distance
  const float densityScale = 1.f / m_unit_distance;
  CHECK(rprMaterialNodeSetInputNByKey(
      m_volume_material, RPR_MATERIAL_INPUT_DENSITYGRID, m_density_grid_material))
  CHECK(rprMaterialNodeSetInputFByKey(
      m_volume_material, RPR_MATERIAL_INPUT_DENSITY, densityScale, densityScale, densityScale, 1.f))
  CHECK(rprMaterialNodeSetInputNByKey(m_volume_material, RPR_MATERIAL_INPUT_COLOR, m_color_material))

  // The RPR volume fills the unit cube around the origin of its shape. The samples of the field are the
  // centers of the voxels of the grid.
  const float3 extent = float3(size) * spacing;
  const float3 center = m_field->origin() + (float3(size) - 1.f) * spacing * 0.5f;
  m_transform         = linalg::mul(linalg::translation_matrix(center), linalg::scaling_matrix(extent));
}

float TransferFunction1DVolume::processDensity(float value) const
{
  // The opacities are the array 'opacity' or, without it, the alpha values of the colors.
  const Array1D *densityLookup = m_opacity_data.get();
  const bool     hasOpacities =
      densityLookup && densityLookup->size() > 0 && densityLookup->elementType() == ANARI_FLOAT32;
  const Array1D *colorLookup = m_color_data.get();
  const bool     hasColors =
      !hasOpacities && colorLookup && colorLookup->size() > 0 && colorLookup->elementType() == ANARI_FLOAT32_VEC4;
  if (!hasOpacities && !hasColors)
    return m_opacity * m_color.w;

  const size_t count  = hasOpacities ? densityLookup->size() : colorLookup->size();
  auto         lookup = [&](size_t i) {
    return hasOpacities ? densityLookup->beginAs<float>()[i] : colorLookup->beginAs<float4>()[i].w;
  };
  if (count == 1) // only one value in lookup table
    return lookup(0);

  // linear interpolation
  const float  position  = std::clamp(value, 0.f, 1.f) * float(count - 1);
  const size_t lookupIdx = std::min(size_t(position), count - 2);
  const float  fraction  = position - float(lookupIdx);
  return lookup(lookupIdx) + (lookup(lookupIdx + 1) - lookup(lookupIdx)) * fraction;
}

void TransferFunction1DVolume::attachToScene(rpr_scene scene, const mat4 &transform, SceneItems &items)
{
  if (!m_volume_material)
    return;

  // Volume shapes don't need any vertices data: the bounds of volume will only be defined by the grid.
  rpr_shape     shape = nullptr;
  rpr_mesh_info mesh_properties[16];
  mesh_properties[0] = (rpr_mesh_info)RPR_MESH_VOLUME_FLAG;
  mesh_properties[1] = (rpr_mesh_info)1; // enable the Volume flag for the Mesh
  mesh_properties[2] = (rpr_mesh_info)0;
  CHECK(rprContextCreateMeshEx2(deviceState()->context, nullptr, 0, 0, nullptr, 0, 0, nullptr, 0, 0, 0, nullptr,
      nullptr, nullptr, nullptr, 0, nullptr, 0, nullptr, nullptr, nullptr, 0, mesh_properties, &shape))
  if (!shape)
    return;
  CHECK(rprShapeSetVolumeMaterial(shape, m_volume_material))
  const mat4 transformResult = linalg::mul(transform, m_transform);
  CHECK(rprShapeSetTransform(shape, false, &transformResult[0][0]))
  CHECK(rprSceneAttachShape(scene, shape))
  items.shapes.push_back(shape);
}

void TransferFunction1DVolume::clear()
{
  auto *state = deviceState();
  state->deleteLater(m_volume_material, DeleteOrder::MATERIAL);
  state->deleteLater(m_color_material, DeleteOrder::MATERIAL);
  state->deleteLater(m_color_grid_material, DeleteOrder::MATERIAL);
  state->deleteLater(m_density_grid_material, DeleteOrder::MATERIAL);
  state->deleteLater(m_color_map, DeleteOrder::DATA);
  state->deleteLater(m_color_grid, DeleteOrder::DATA);
  state->deleteLater(m_density_grid, DeleteOrder::DATA);
  m_volume_material       = nullptr;
  m_color_material        = nullptr;
  m_color_grid_material   = nullptr;
  m_density_grid_material = nullptr;
  m_color_map             = nullptr;
  m_color_grid            = nullptr;
  m_density_grid          = nullptr;
}

TransferFunction1DVolume::~TransferFunction1DVolume()
{
  clear();
}

} // namespace anari::rpr
