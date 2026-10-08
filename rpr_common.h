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

#include "RadeonProRender.h"

namespace anari::rpr {

struct RPRGlobalState;

// Reports a failed RPR call through the ANARI status callback, returns true on success
bool checkStatus(const RPRGlobalState *state, rpr_status status, const char *call, const char *file, int line);

} // namespace anari::rpr

// Checks the status of a RPR call. It can be used in every scope where 'deviceState()' is available.
// A failed call is reported as ANARI error, it never stops the application.
#define CHECK(x)                                                                                             \
  {                                                                                                          \
    ::anari::rpr::checkStatus(deviceState(), (x), #x, __FILE__, __LINE__);                                   \
  }
