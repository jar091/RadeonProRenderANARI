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

#include "RPRGlobalState.h"
// helium
#include "helium/BaseObject.h"
// std
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <system_error>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace anari::rpr {

RPRGlobalState::RPRGlobalState(ANARIDevice d) : helium::BaseGlobalDeviceState(d) {}

void RPRGlobalState::deleteLater(void *rprObject, DeleteOrder order)
{
  if (!rprObject)
    return;
  std::lock_guard<std::mutex> lock(m_garbageMutex);
  m_garbage.emplace_back(order, rprObject);
}

void RPRGlobalState::collectGarbage()
{
  std::vector<std::pair<DeleteOrder, void *>> garbage;
  {
    std::lock_guard<std::mutex> lock(m_garbageMutex);
    garbage.swap(m_garbage);
  }
  if (garbage.empty())
    return;

  // objects which use other objects are deleted first
  std::stable_sort(
      garbage.begin(), garbage.end(), [](const auto &a, const auto &b) { return a.first < b.first; });
  for (auto &g : garbage)
    deleteObject(g.second, "rprObjectDelete");
}

void RPRGlobalState::deleteObject(void *rprObject, const char *name) const
{
  if (!rprObject || contextFailed)
    return;
  // exceptions of RPR must not leave the destructors of the device and its objects
  try
  {
    checkStatus(this, rprObjectDelete(rprObject), name, __FILE__, __LINE__);
  } catch (...)
  {
    if (messageFunction)
      messageFunction(ANARI_SEVERITY_ERROR, std::string("exception in ") + name, ANARI_DEVICE, anariDevice);
  }
}

bool checkStatus(const RPRGlobalState *state, rpr_status status, const char *call, const char *file, int line)
{
  if (status == RPR_SUCCESS)
    return true;

  std::string message;
  if (state && state->context)
  {
    size_t size = 0;
    if (rprContextGetInfo(state->context, RPR_CONTEXT_LAST_ERROR_MESSAGE, 0, nullptr, &size) == RPR_SUCCESS
        && size > 1)
    {
      message.resize(size);
      rprContextGetInfo(state->context, RPR_CONTEXT_LAST_ERROR_MESSAGE, size, message.data(), nullptr);
      message.resize(size - 1);
    }
  }

  // only the file name, not the path of the build machine
  std::string fileName = file;
  const auto  separator = fileName.find_last_of("\\/");
  if (separator != std::string::npos)
    fileName = fileName.substr(separator + 1);

  const std::string text = helium::string_printf("RPR error %d%s%s%s in '%s' (%s:%d)", int(status),
      message.empty() ? "" : " (", message.c_str(), message.empty() ? "" : ")", call, fileName.c_str(), line);
  if (state && state->messageFunction)
    state->messageFunction(ANARI_SEVERITY_ERROR, text, ANARI_DEVICE, state->anariDevice);
  else
    fprintf(stderr, "[rpr] %s\n", text.c_str());
  return false;
}

std::string deviceLibraryDirectory()
{
  std::string path;
#ifdef _WIN32
  HMODULE self = nullptr;
  if (!GetModuleHandleExA(
          GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
          (LPCSTR)&deviceLibraryDirectory, &self))
    return path;

  path.resize(MAX_PATH);
  DWORD length = GetModuleFileNameA(self, path.data(), DWORD(path.size()));
  while (length == path.size())
  {
    path.resize(path.size() * 2);
    length = GetModuleFileNameA(self, path.data(), DWORD(path.size()));
  }
  path.resize(length);
#else
  Dl_info info;
  if (dladdr((void *)&deviceLibraryDirectory, &info) && info.dli_fname)
    path = info.dli_fname;
#endif
  const auto separator = path.find_last_of("\\/");
  if (separator == std::string::npos)
    return "";
  path.resize(separator + 1);
  return path;
}

bool loadRuntimeLibrary(const char *name)
{
#ifdef _WIN32
  const std::string path = deviceLibraryDirectory() + name;
  if (LoadLibraryExA(path.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH))
    return true;
  return LoadLibraryA(name) != nullptr;
#else
  return dlopen(name, RTLD_NOW | RTLD_GLOBAL) != nullptr;
#endif
}

std::string cacheDirectory(const std::string &path)
{
  namespace fs = std::filesystem;

  // a directory is usable if it exists or can be created and a file can be written to it
  auto isWritable = [](const fs::path &directory) {
    std::error_code error;
    fs::create_directories(directory, error);
    if (!fs::is_directory(directory, error))
      return false;
    const fs::path file = directory / "rpr_anari_write_test";
    FILE          *f    = fopen(file.string().c_str(), "wb");
    if (!f)
      return false;
    fclose(f);
    fs::remove(file, error);
    return true;
  };

  std::vector<fs::path> candidates;
  if (!path.empty())
    candidates.push_back(path);
  const std::string libraryDirectory = deviceLibraryDirectory();
  if (!libraryDirectory.empty())
    candidates.push_back(fs::path(libraryDirectory) / "cache");
  std::error_code error;
  const fs::path  temp = fs::temp_directory_path(error);
  if (!error)
    candidates.push_back(temp / "rpr_anari_cache");

  for (const fs::path &candidate : candidates)
  {
    if (isWritable(candidate))
      return candidate.string();
  }
  return "";
}

rpr_status createContextGuarded(rpr_int *plugins, size_t pluginCount, rpr_creation_flags flags,
    const rpr_context_properties *properties, const char *cachePath, rpr_context *context)
{
#ifdef _MSC_VER
  // Northstar calls functions of the GPU runtime without checking that they exist, for example if the
  // CUDA runtime it was built for is not installed. Structured exception handling turns such a crash into
  // an error, so the device can fall back to another backend.
  __try
  {
    return rprCreateContext(RPR_API_VERSION, plugins, pluginCount, flags, properties, cachePath, context);
  } __except (EXCEPTION_EXECUTE_HANDLER)
  {
    *context = nullptr;
    return RPR_ERROR_INTERNAL_ERROR;
  }
#else
  return rprCreateContext(RPR_API_VERSION, plugins, pluginCount, flags, properties, cachePath, context);
#endif
}

void loadRPRLibrary()
{
#ifdef _WIN32
  // RadeonProRender64.dll is delay loaded. The host application can be anywhere (for example a DCC
  // tool with its own set of libraries), so load it from the directory of the device library before
  // the first RPR call instead of relying on the search path of the application.
  static std::once_flag once;
  std::call_once(once, []() {
    const std::string path = deviceLibraryDirectory() + "RadeonProRender64.dll";
    // Not finding it here is fine: the delay load helper then searches as usual.
    LoadLibraryExA(path.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
  });
#endif
}

} // namespace anari::rpr
