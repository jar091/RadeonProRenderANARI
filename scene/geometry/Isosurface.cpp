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

#include "Isosurface.h"
// std
#include <vector>

namespace anari::rpr {

Isosurface::Isosurface(RPRGlobalState *s) : Geometry(s), m_field(this), m_isovalue_data(this) {}

void Isosurface::commitParameters()
{
  Geometry::commitParameters();
  // the field can be a placeholder for an unsupported subtype
  m_field         = dynamic_cast<SpatialField *>(getParamObject<helium::BaseObject>("field"));
  m_isovalue_data = getArray1D(this, "isovalue");
  m_has_isovalue  = getParam("isovalue", ANARI_FLOAT32, &m_isovalue);
}

void Isosurface::finalize()
{
  clearMesh();

  if (!m_field || !m_field->isValid())
  {
    reportMessage(ANARI_SEVERITY_WARNING, "isosurface geometry has no supported spatial field 'field'");
    return;
  }

  std::vector<float> isovalues;
  if (m_isovalue_data && m_isovalue_data->elementType() == ANARI_FLOAT32)
    isovalues.assign(m_isovalue_data->beginAs<float>(), m_isovalue_data->endAs<float>());
  else if (m_has_isovalue)
    isovalues.push_back(m_isovalue);
  if (isovalues.empty())
  {
    reportMessage(ANARI_SEVERITY_WARNING, "missing 'isovalue' on isosurface geometry");
    return;
  }

  // The surface of a coarse field is smooth in other renderers, which interpolate the field. So the field
  // is resampled to a finer grid.
  const uint32_t           factor = SpatialField::resamplingFactor(m_field->size(), 64);
  uint3                    size(0u);
  const std::vector<float> values  = m_field->resample(factor, size);
  const float3             spacing = m_field->spacing() / float(factor);
  const float3             origin  = m_field->origin();
  if (size.x < 2 || size.y < 2 || size.z < 2)
    return;

  auto value = [&](uint32_t x, uint32_t y, uint32_t z) {
    return values[size_t(x) + size_t(size.x) * (size_t(y) + size_t(size.y) * size_t(z))];
  };
  // gradient of the field at a sample of the grid (in units of the grid)
  auto gradient = [&](uint32_t x, uint32_t y, uint32_t z) {
    const uint32_t x0 = x > 0 ? x - 1 : x, x1 = x + 1 < size.x ? x + 1 : x;
    const uint32_t y0 = y > 0 ? y - 1 : y, y1 = y + 1 < size.y ? y + 1 : y;
    const uint32_t z0 = z > 0 ? z - 1 : z, z1 = z + 1 < size.z ? z + 1 : z;
    return float3((value(x1, y, z) - value(x0, y, z)) / float(x1 - x0),
        (value(x, y1, z) - value(x, y0, z)) / float(y1 - y0), (value(x, y, z1) - value(x, y, z0)) / float(z1 - z0));
  };

  // Marching tetrahedra: every cell of the grid is split into six tetrahedra around its diagonal.
  static const uint32_t cornerOffsets[8][3] = {
      {0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0}, {0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1}};
  static const int tetrahedra[6][4] = {
      {0, 5, 1, 6}, {0, 1, 2, 6}, {0, 2, 3, 6}, {0, 3, 7, 6}, {0, 7, 4, 6}, {0, 4, 5, 6}};

  MeshData mesh;

  struct Corner
  {
    float3 position;
    float3 gradient;
    float  value;
  };

  for (float isovalue : isovalues)
  {
    // point of the surface on the edge between two corners with its normal
    auto addVertex = [&](const Corner &a, const Corner &b) {
      const float  difference = b.value - a.value;
      const float  t          = difference != 0.f ? std::clamp((isovalue - a.value) / difference, 0.f, 1.f) : 0.5f;
      const float3 g          = linalg::lerp(a.gradient, b.gradient, t) / spacing;
      mesh.indices.push_back(rpr_int(mesh.positions.size()));
      mesh.positions.push_back(linalg::lerp(a.position, b.position, t));
      // the normal points to the side of the smaller values
      mesh.normals.push_back(safeNormalize(-g, float3(0.f, 0.f, 1.f)));
    };

    for (uint32_t z = 0; z + 1 < size.z; z++)
    {
      for (uint32_t y = 0; y + 1 < size.y; y++)
      {
        for (uint32_t x = 0; x + 1 < size.x; x++)
        {
          // skip cells which are not intersected by the surface
          float cellValues[8];
          bool  anyInside = false, anyOutside = false;
          for (int c = 0; c < 8; c++)
          {
            cellValues[c] = value(x + cornerOffsets[c][0], y + cornerOffsets[c][1], z + cornerOffsets[c][2]);
            anyInside |= cellValues[c] > isovalue;
            anyOutside |= !(cellValues[c] > isovalue);
          }
          if (!anyInside || !anyOutside)
            continue;

          Corner corners[8];
          for (int c = 0; c < 8; c++)
          {
            const uint32_t cx = x + cornerOffsets[c][0], cy = y + cornerOffsets[c][1], cz = z + cornerOffsets[c][2];
            corners[c].position = origin + float3(float(cx), float(cy), float(cz)) * spacing;
            corners[c].gradient = gradient(cx, cy, cz);
            corners[c].value    = cellValues[c];
          }

          for (const auto &tetrahedron : tetrahedra)
          {
            // corners inside (above the isovalue) and outside of the surface
            int inside[4], outside[4];
            int numInside = 0, numOutside = 0;
            for (int i = 0; i < 4; i++)
            {
              if (corners[tetrahedron[i]].value > isovalue)
                inside[numInside++] = tetrahedron[i];
              else
                outside[numOutside++] = tetrahedron[i];
            }
            if (numInside == 1)
            {
              for (int i = 0; i < 3; i++)
                addVertex(corners[inside[0]], corners[outside[i]]);
            } else if (numInside == 3)
            {
              for (int i = 0; i < 3; i++)
                addVertex(corners[outside[0]], corners[inside[i]]);
            } else if (numInside == 2)
            {
              // a quad between the four edges from the corners inside to the corners outside
              addVertex(corners[inside[0]], corners[outside[0]]);
              addVertex(corners[inside[0]], corners[outside[1]]);
              addVertex(corners[inside[1]], corners[outside[1]]);
              addVertex(corners[inside[0]], corners[outside[0]]);
              addVertex(corners[inside[1]], corners[outside[1]]);
              addVertex(corners[inside[1]], corners[outside[0]]);
            }
          }
        }
      }
    }
  }

  if (mesh.positions.empty())
  {
    reportMessage(ANARI_SEVERITY_DEBUG, "isosurface geometry is empty");
    return;
  }
  setMesh(mesh.view());
}

} // namespace anari::rpr
