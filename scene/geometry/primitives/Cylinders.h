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

#include "Primitives.h"

namespace anari::rpr {

struct Cylinders : public Primitives
{
  explicit Cylinders(RPRGlobalState *s);
  void commitParameters() override;
  void finalize() override;

 private:
  helium::ChangeObserverPtr<Array1D> m_radius;
  float                              m_global_radius{1.f};
  std::string                        m_caps;
};

} // namespace anari::rpr
