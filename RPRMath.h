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

// helium
#include "helium/helium_math.h"
// std
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <optional>

namespace anari::rpr {

using namespace anari::math;
using namespace helium::math;

constexpr float pi = 3.14159265358979323846f;

// attribute slots of a geometry: attribute0..attribute3 and color
constexpr int NUM_ATTRIBUTES = 5;
constexpr int ATTRIBUTE_COLOR = 4;

using UniformAttributeSet = std::array<std::optional<float4>, NUM_ATTRIBUTES>;

// Helper functions ///////////////////////////////////////////////////////////

inline box3 emptyBox()
{
  return box3(float3(std::numeric_limits<float>::max()), float3(-std::numeric_limits<float>::max()));
}

inline bool isEmpty(const box3 &b)
{
  return b.lower.x > b.upper.x || b.lower.y > b.upper.y || b.lower.z > b.upper.z;
}

inline float3 xfmPoint(const mat4 &m, const float3 &p)
{
  const float4 r = linalg::mul(m, float4(p, 1.f));
  return float3(r.x, r.y, r.z);
}

inline float3 xfmVec(const mat4 &m, const float3 &v)
{
  const float4 r = linalg::mul(m, float4(v, 0.f));
  return float3(r.x, r.y, r.z);
}

inline box3 xfmBox(const mat4 &m, const box3 &b)
{
  box3 retval = emptyBox();
  if (isEmpty(b))
    return retval;
  for (int i = 0; i < 8; i++)
  {
    const float3 corner(
        (i & 1) ? b.upper.x : b.lower.x, (i & 2) ? b.upper.y : b.lower.y, (i & 4) ? b.upper.z : b.lower.z);
    retval.extend(xfmPoint(m, corner));
  }
  return retval;
}

inline float3 safeNormalize(const float3 &v, const float3 &fallback)
{
  const float l = linalg::length(v);
  return l > 0.f && std::isfinite(l) ? v / l : fallback;
}

// builds two vectors orthogonal to the normalized vector 'n'
inline void makeBasis(const float3 &n, float3 &u, float3 &v)
{
  const float3 helper = std::abs(n.x) < 0.9f ? float3(1.f, 0.f, 0.f) : float3(0.f, 1.f, 0.f);
  u                   = linalg::normalize(linalg::cross(helper, n));
  v                   = linalg::cross(n, u);
}

// transformation of a local frame: the columns are the images of the x, y and z axis and the origin
inline mat4 frameMatrix(const float3 &x, const float3 &y, const float3 &z, const float3 &origin)
{
  return mat4(float4(x, 0.f), float4(y, 0.f), float4(z, 0.f), float4(origin, 1.f));
}

// rotation which maps the direction (0, 0, -1) to 'direction'
inline mat4 rotationFromDirection(const float3 &direction, const float3 &origin = float3(0.f))
{
  const float3 z = -safeNormalize(direction, float3(0.f, 0.f, -1.f));
  float3       x, y;
  makeBasis(z, x, y);
  return frameMatrix(x, y, z, origin);
}

inline bool isIdentity(const mat4 &m)
{
  return m == mat4(linalg::identity);
}

} // namespace anari::rpr
