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

#include "StucturedRegularSpatialField.h"

namespace anari::rpr {

StructuredRegularSpatialField::StructuredRegularSpatialField(RPRGlobalState *s) : SpatialField(s), m_data(this)
{}

void StructuredRegularSpatialField::commitParameters()
{
  m_data      = getArray3D(this, "data");
  m_position  = getParam<float3>("origin", float3(0.f));
  m_voxelSize = getParam<float3>("spacing", float3(1.f));
}

void StructuredRegularSpatialField::finalize()
{
  resetBounds();
  m_valid = false;

  if (!m_data)
  {
    reportMessage(ANARI_SEVERITY_WARNING, "'data' parameter is required");
    return;
  }

  switch (m_data->elementType())
  {
  case ANARI_UINT8:
  case ANARI_UFIXED8:
  case ANARI_INT16:
  case ANARI_UINT16:
  case ANARI_UFIXED16:
  case ANARI_FLOAT32:
  case ANARI_FLOAT64:
    break;
  default:
    reportMessage(ANARI_SEVERITY_WARNING, "unsupported element type %s of 'data' of structuredRegular field",
        anari::toString(m_data->elementType()));
    return;
  }

  const uint3 dims = m_data->size();
  if (dims.x == 0 || dims.y == 0 || dims.z == 0)
    return;

  m_valid = true;
  extendBounds(box3(m_position, m_position + (float3(dims) - 1.f) * m_voxelSize));
}

bool StructuredRegularSpatialField::isValid() const
{
  return m_valid;
}

uint3 StructuredRegularSpatialField::size() const
{
  return m_data->size();
}

float StructuredRegularSpatialField::value(size_t index) const
{
  const void *data = m_data->data();
  switch (m_data->elementType())
  {
  case ANARI_UINT8:
    return float(((const uint8_t *)data)[index]);
  case ANARI_UFIXED8:
    return float(((const uint8_t *)data)[index]) / 255.f;
  case ANARI_INT16:
    return float(((const int16_t *)data)[index]);
  case ANARI_UINT16:
    return float(((const uint16_t *)data)[index]);
  case ANARI_UFIXED16:
    return float(((const uint16_t *)data)[index]) / 65535.f;
  case ANARI_FLOAT32:
    return ((const float *)data)[index];
  case ANARI_FLOAT64:
    return float(((const double *)data)[index]);
  default:
    return 0.f;
  }
}

float3 StructuredRegularSpatialField::origin() const
{
  return m_position;
}

float3 StructuredRegularSpatialField::spacing() const
{
  return m_voxelSize;
}

} // namespace anari::rpr
