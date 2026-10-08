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

#include "Attribute.h"

namespace anari::rpr {

const char *attributeName(int slot)
{
  static const char *names[NUM_ATTRIBUTES] = {"attribute0", "attribute1", "attribute2", "attribute3", "color"};
  return slot >= 0 && slot < NUM_ATTRIBUTES ? names[slot] : "";
}

bool AttributeContext::operator<(const AttributeContext &other) const
{
  for (int i = 0; i < NUM_ATTRIBUTES; i++)
  {
    if (buffer[i] != other.buffer[i])
      return buffer[i] < other.buffer[i];
    const auto &a = constant[i];
    const auto &b = other.constant[i];
    if (a.has_value() != b.has_value())
      return !a.has_value();
    if (!a.has_value())
      continue;
    for (int c = 0; c < 4; c++)
    {
      if ((*a)[c] != (*b)[c])
        return (*a)[c] < (*b)[c];
    }
  }
  return false;
}

} // namespace anari::rpr
