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

#include "../camera/Camera.h"
#include "../renderer/Renderer.h"
#include "../rpr_common.h"
#include "../scene/World.h"
// helium
#include "helium/BaseFrame.h"
#include "helium/TaskQueue.h"
// std
#include <memory>
#include <vector>

namespace anari {
namespace rpr {

struct Frame : public helium::BaseFrame
{
  explicit Frame(RPRGlobalState *s);
  ~Frame() override;

  bool isValid() const override;

  RPRGlobalState *deviceState() const;

  bool getProperty(
      const std::string_view &name, ANARIDataType type, void *ptr, uint64_t size, uint32_t flags) override;

  void commitParameters() override;
  void finalize() override;

  void renderFrame() override;

  void *map(std::string_view channel, uint32_t *width, uint32_t *height, ANARIDataType *pixelType) override;
  void  unmap(std::string_view channel) override;
  int   frameReady(ANARIWaitMask m) override;
  void  discard() override;

  bool ready() const;
  void wait();

 private:
  void waitOnOutstandingWorkIfNeeded();

  // renders the frame, has to be called by the render thread
  void render();
  bool createFramebuffers();
  void deleteFramebuffers();
  // copies the result of RPR to the buffers of the channels
  void resolve(bool visibleEnvironment);
  // empty image for frames which could not be rendered
  void clearBuffers();
  // creates the background image of the RPR scene from the background of the renderer
  void updateBackgroundImage();

  // RPR framebuffer of an AOV and the buffer it is resolved to
  struct AOV
  {
    rpr_framebuffer    framebuffer{};
    rpr_framebuffer    resolved{};
    std::vector<float4> data;
  };
  bool createAOV(AOV &aov);
  void deleteAOV(AOV &aov);
  void resolveAOV(AOV &aov, bool isColor);

  // Data //
  ANARIDataType m_color_type{ANARI_UNKNOWN};
  ANARIDataType m_depth_type{ANARI_UNKNOWN};
  uint2         m_size{0u, 0u};
  bool          m_accumulation{false};

  // size of the RPR framebuffers
  uint2 m_framebuffer_size{0u, 0u};
  bool  m_framebuffer_has_depth{false};

  float m_duration{0.f};

  std::vector<uint8_t> m_pixel_buffer;
  std::vector<float>   m_depth_buffer;

  helium::IntrusivePtr<Renderer> m_renderer;
  helium::IntrusivePtr<Camera>   m_camera;
  helium::IntrusivePtr<World>    m_world;

  mutable helium::tasking::Future m_future;
  ANARIFrameCompletionCallback    m_callback{nullptr};
  const void                     *m_callback_user_ptr{nullptr};

  // the accumulated samples are discarded if anything was changed after this time stamp
  helium::TimeStamp m_last_rendered{0};
  bool              m_frame_changed{true};
  uint32_t          m_frame_count{0}; // number of frames rendered by this object

  AOV m_color;
  AOV m_opacity;
  AOV m_position;

  // background of the renderer as image with the size of the frame
  rpr_image         m_background_image{};
  uint2             m_background_size{0u, 0u};
  const Renderer   *m_background_renderer{nullptr};
  helium::TimeStamp m_background_updated{0};
};

} // namespace rpr

ANARI_TYPEFOR_SPECIALIZATION(rpr::Frame *, ANARI_FRAME)

} // namespace anari
