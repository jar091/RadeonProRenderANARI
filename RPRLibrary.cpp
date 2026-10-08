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

#include "RPRDevice.h"
// anari
#include "anari/backend/LibraryImpl.h"

namespace anari_rpr {

const char **query_extensions();

} // namespace anari_rpr

namespace anari::rpr {

struct RPRLibrary : public anari::LibraryImpl
{
  RPRLibrary(void *lib, ANARIStatusCallback defaultStatusCB, const void *statusCBPtr);

  ANARIDevice  newDevice(const char *subtype) override;
  const char **getDeviceExtensions(const char *deviceType) override;
};

// Definitions ////////////////////////////////////////////////////////////////

RPRLibrary::RPRLibrary(void *lib, ANARIStatusCallback defaultStatusCB, const void *statusCBPtr)
    : anari::LibraryImpl(lib, defaultStatusCB, statusCBPtr)
{}

ANARIDevice RPRLibrary::newDevice(const char * /*subtype*/)
{
  // there is a single device, it is created for every subtype ('default', 'mpi', ...)
  return (ANARIDevice) new RPRDevice(this_library());
}

const char **RPRLibrary::getDeviceExtensions(const char * /*deviceType*/)
{
  return anari_rpr::query_extensions();
}

} // namespace anari::rpr

// Define library entrypoint //////////////////////////////////////////////////

extern "C" RPR_DEVICE_INTERFACE ANARI_DEFINE_LIBRARY_ENTRYPOINT(rpr, handle, scb, scbPtr)
{
  return (ANARILibrary) new anari::rpr::RPRLibrary(handle, scb, scbPtr);
}

extern "C" RPR_DEVICE_INTERFACE ANARIDevice anariNewRPRDevice(
    ANARIStatusCallback defaultCallback, const void *userPtr)
{
  return (ANARIDevice) new anari::rpr::RPRDevice(defaultCallback, userPtr);
}
