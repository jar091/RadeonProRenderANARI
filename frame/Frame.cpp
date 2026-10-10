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

#include "Frame.h"
// std
#include <algorithm>
#include <chrono>

namespace anari {
namespace rpr {

// RPR framebuffers start with the lower row of the image (RPR_CONTEXT_Y_FLIP is off) like ANARI frames
constexpr bool FLIP_FRAMEBUFFER_ROWS = false;

// Frame definitions //////////////////////////////////////////////////////////

Frame::Frame(RPRGlobalState *s) : helium::BaseFrame(s) {}

Frame::~Frame()
{
  wait();
  deleteFramebuffers();
  deviceState()->deleteLater(m_background_image, DeleteOrder::DATA);
}

bool Frame::isValid() const
{
  return m_renderer && m_renderer->isValid() && m_camera && m_camera->isValid() && m_world && m_world->isValid()
      && m_size.x > 0 && m_size.y > 0;
}

RPRGlobalState *Frame::deviceState() const
{
  return (RPRGlobalState *)helium::BaseObject::m_state;
}

void Frame::commitParameters()
{
  waitOnOutstandingWorkIfNeeded();

  m_renderer = getObjectParam<Renderer>(this, "renderer", ANARI_RENDERER);
  m_camera   = getObjectParam<Camera>(this, "camera", ANARI_CAMERA);
  m_world    = getObjectParam<World>(this, "world", ANARI_WORLD);

  m_color_type   = getParam<anari::DataType>("channel.color", ANARI_UNKNOWN);
  m_depth_type   = getParam<anari::DataType>("channel.depth", ANARI_UNKNOWN);
  m_size         = getParam<uint2>("size", uint2(0u));
  m_accumulation = getParam<bool>("accumulation", false);

  m_callback          = getParam<ANARIFrameCompletionCallback>("frameCompletionCallback", nullptr);
  m_callback_user_ptr = getParam<void *>("frameCompletionCallbackUserData", nullptr);
}

void Frame::finalize()
{
  waitOnOutstandingWorkIfNeeded();

  if (!m_renderer)
    reportMessage(ANARI_SEVERITY_WARNING, "missing required parameter 'renderer' on frame");
  if (!m_camera)
    reportMessage(ANARI_SEVERITY_WARNING, "missing required parameter 'camera' on frame");
  if (!m_world)
    reportMessage(ANARI_SEVERITY_WARNING, "missing required parameter 'world' on frame");
  if (m_size.x == 0 || m_size.y == 0)
    reportMessage(ANARI_SEVERITY_WARNING, "invalid frame dimensions");

  if (m_color_type != ANARI_UNKNOWN && m_color_type != ANARI_UFIXED8_VEC4
      && m_color_type != ANARI_UFIXED8_RGBA_SRGB && m_color_type != ANARI_FLOAT32_VEC4)
  {
    reportMessage(ANARI_SEVERITY_WARNING, "unsupported type %s of 'channel.color', %s is used instead",
        anari::toString(m_color_type), anari::toString(ANARI_FLOAT32_VEC4));
    m_color_type = ANARI_FLOAT32_VEC4;
  }
  if (m_depth_type != ANARI_UNKNOWN && m_depth_type != ANARI_FLOAT32)
  {
    reportMessage(ANARI_SEVERITY_WARNING, "unsupported type %s of 'channel.depth', %s is used instead",
        anari::toString(m_depth_type), anari::toString(ANARI_FLOAT32));
    m_depth_type = ANARI_FLOAT32;
  }
  static const char *unsupported[] = {"channel.primitiveId", "channel.objectId", "channel.instanceId",
      "channel.normal", "channel.albedo"};
  for (const char *channel : unsupported)
  {
    if (getParam<anari::DataType>(channel, ANARI_UNKNOWN) != ANARI_UNKNOWN)
      reportMessage(ANARI_SEVERITY_WARNING, "frame channel '%s' is not supported", channel);
  }

  const size_t numPixels = size_t(m_size.x) * size_t(m_size.y);
  m_pixel_buffer.assign(m_color_type == ANARI_UNKNOWN ? 0 : numPixels * anari::sizeOf(m_color_type), 0);
  m_depth_buffer.assign(
      m_depth_type == ANARI_UNKNOWN ? 0 : numPixels, std::numeric_limits<float>::infinity());

  // committing the frame restarts the accumulation
  m_frame_changed = true;
}

bool Frame::getProperty(
    const std::string_view &name, ANARIDataType type, void *ptr, uint64_t size, uint32_t flags)
{
  if (type == ANARI_FLOAT32 && name == "duration")
  {
    if (flags & ANARI_WAIT)
      wait();
    helium::writeToVoidP(ptr, m_duration);
    return true;
  }
  if (type == ANARI_BOOL && name == "valid")
  {
    helium::writeToVoidP(ptr, isValid());
    return true;
  }

  return false;
}

void Frame::renderFrame()
{
  // m_future tracks the job running the callback, and an app thread may be
  // blocked on it; replacing it from here would race with that wait.
  if (completingOnThisThread())
  {
    reportMessage(ANARI_SEVERITY_WARNING, "rendering a frame from its own completion callback is not supported");
    return;
  }

  auto *state = deviceState();
  wait();

  this->refInc(helium::RefType::INTERNAL);

  m_future = state->taskQueue.enqueue([this, state]() {
    const auto start = std::chrono::steady_clock::now();
    {
      // RPR objects are created, updated and rendered by this thread only while the mutex is locked
      std::lock_guard<std::recursive_mutex> lock(state->rprMutex);
      // an exception of the render plugin must not terminate the application
      try
      {
        state->commitBuffer.flush();
        render();
        state->collectGarbage();
      } catch (const std::exception &e)
      {
        reportMessage(ANARI_SEVERITY_ERROR, "exception while rendering the frame: %s", e.what());
        clearBuffers();
        m_frame_changed = true;
      } catch (...)
      {
        reportMessage(ANARI_SEVERITY_ERROR, "unknown exception while rendering the frame");
        clearBuffers();
        m_frame_changed = true;
      }
    }
    const auto end = std::chrono::steady_clock::now();
    m_duration     = std::chrono::duration<float>(end - start).count();

    invokeCompletionCallback(m_callback, m_callback_user_ptr, state->anariDevice);
  });
}

void Frame::render()
{
  auto *state = deviceState();

  if (!isValid() || !state->context)
  {
    reportMessage(ANARI_SEVERITY_ERROR, "skipping render of incomplete frame object");
    clearBuffers();
    return;
  }
  if (state->contextFailed)
  {
    reportMessage(ANARI_SEVERITY_ERROR, "skipping render: RadeonProRender failed to render a previous frame");
    clearBuffers();
    return;
  }

  if (!createFramebuffers())
    return;

  // bring the scene up to date
  rpr_scene scene = m_world->update(*m_renderer);
  if (!scene)
    return;
  m_camera->attachToScene(scene, m_size);
  m_renderer->attachToContext();

  const bool visibleEnvironment = m_world->hasVisibleEnvironment();

  rpr_context context = state->context;
  CHECK(rprContextSetScene(context, scene))
  // The background of the renderer is the background image of the RPR scene, so RPR blends transparent
  // surfaces and volumes with it. A visible environment light replaces the background of the renderer.
  if (visibleEnvironment)
  {
    CHECK(rprSceneSetBackgroundImage(scene, nullptr))
  } else
  {
    updateBackgroundImage();
    CHECK(rprSceneSetBackgroundImage(scene, m_background_image))
  }
  CHECK(rprContextSetParameterByKey1u(context, RPR_CONTEXT_TRANSPARENT_BACKGROUND, visibleEnvironment ? 0 : 1))
  CHECK(rprContextSetAOV(context, RPR_AOV_COLOR, m_color.framebuffer))
  CHECK(rprContextSetAOV(context, RPR_AOV_OPACITY, m_opacity.framebuffer))
  CHECK(rprContextSetAOV(context, RPR_AOV_WORLD_COORDINATE, m_position.framebuffer))

  // The samples of the previous frames are kept if the frame accumulates and nothing has changed.
  const bool changed = m_frame_changed || state->commitBuffer.lastObjectFinalization() > m_last_rendered;
  if (changed || !m_accumulation)
  {
    CHECK(rprFrameBufferClear(m_color.framebuffer))
    CHECK(rprFrameBufferClear(m_opacity.framebuffer))
    if (m_position.framebuffer)
    {
      CHECK(rprFrameBufferClear(m_position.framebuffer))
    }
    // RPR starts the same sequence of random numbers after the framebuffers were cleared. Frames without
    // accumulation have to be independent images, so every one of them gets its own seed: an application
    // which averages the frames gets a converging image.
    CHECK(rprContextSetParameterByKey1u(context, RPR_CONTEXT_RANDOM_SEED, m_accumulation ? 0u : m_frame_count))
  }
  m_frame_count++;
  m_frame_changed = false;
  m_last_rendered = helium::newTimeStamp();

  const auto renderStart = std::chrono::steady_clock::now();

  // RPR reports some errors as exceptions, for example if the compilation of its kernels fails
  rpr_status status = RPR_ERROR_INTERNAL_ERROR;
  try
  {
    status = rprContextRender(context);
  } catch (const std::exception &e)
  {
    reportMessage(ANARI_SEVERITY_ERROR, "exception in rprContextRender: %s", e.what());
    state->contextFailed = true;
  } catch (...)
  {
    reportMessage(ANARI_SEVERITY_ERROR, "unknown exception in rprContextRender");
    state->contextFailed = true;
  }
  if (status == RPR_SUCCESS)
  {
    reportMessage(ANARI_SEVERITY_DEBUG, "rendered %d samples per pixel in %.2f s%s", m_renderer->pixelSamples(),
        std::chrono::duration<float>(std::chrono::steady_clock::now() - renderStart).count(),
        changed ? " (including the update of the scene)" : "");
    resolve(visibleEnvironment);
  } else
  {
    checkStatus(state, status, "rprContextRender", __FILE__, __LINE__);
    // the frame is empty, render again from scratch next time
    clearBuffers();
    m_frame_changed = true;
  }

  // the framebuffers of this frame must not be used by the frames of other frame objects
  CHECK(rprContextSetAOV(context, RPR_AOV_COLOR, nullptr))
  CHECK(rprContextSetAOV(context, RPR_AOV_OPACITY, nullptr))
  CHECK(rprContextSetAOV(context, RPR_AOV_WORLD_COORDINATE, nullptr))
}

void Frame::updateBackgroundImage()
{
  if (m_background_renderer == m_renderer.ptr && m_background_size == m_size
      && m_renderer->lastUpdated() < m_background_updated)
    return;

  auto *state = deviceState();
  state->deleteLater(m_background_image, DeleteOrder::DATA);
  m_background_image    = nullptr;
  m_background_renderer = m_renderer.ptr;
  m_background_size     = m_size;
  m_background_updated  = helium::newTimeStamp();

  // RPR images start with the upper row. The alpha value of the background is the alpha value of the
  // frame where no object is hit, the colors are not multiplied by it (like in other ANARI devices).
  const size_t        width  = m_size.x;
  const size_t        height = m_size.y;
  std::vector<float4> pixels(width * height);
  bool                isBlack = true;
  for (size_t y = 0; y < height; y++)
  {
    for (size_t x = 0; x < width; x++)
    {
      const float4 background =
          m_renderer->background((float(x) + 0.5f) / float(width), (float(y) + 0.5f) / float(height));
      const float4 color(background.x, background.y, background.z, 1.f);
      isBlack &= color.x <= 0.f && color.y <= 0.f && color.z <= 0.f;
      pixels[(height - 1 - y) * width + x] = color;
    }
  }
  // a black background needs no image
  if (isBlack)
    return;

  rpr_image_desc desc;
  desc.image_width       = rpr_uint(width);
  desc.image_height      = rpr_uint(height);
  desc.image_depth       = 0;
  desc.image_row_pitch   = rpr_uint(width * sizeof(float4));
  desc.image_slice_pitch = 0;
  CHECK(rprContextCreateImage(
      state->context, {4, RPR_COMPONENT_TYPE_FLOAT32}, &desc, pixels.data(), &m_background_image))
  if (m_background_image)
  {
    CHECK(rprImageSetGamma(m_background_image, 1.f))
  }
}

void Frame::clearBuffers()
{
  std::fill(m_pixel_buffer.begin(), m_pixel_buffer.end(), uint8_t(0));
  std::fill(m_depth_buffer.begin(), m_depth_buffer.end(), std::numeric_limits<float>::infinity());
}

bool Frame::createAOV(AOV &aov)
{
  auto *state = deviceState();

  rpr_framebuffer_desc desc;
  desc.fb_width  = m_size.x;
  desc.fb_height = m_size.y;
  const rpr_framebuffer_format format = {4, RPR_COMPONENT_TYPE_FLOAT32};
  CHECK(rprContextCreateFrameBuffer(state->context, format, &desc, &aov.framebuffer))
  CHECK(rprContextCreateFrameBuffer(state->context, format, &desc, &aov.resolved))
  aov.data.assign(size_t(m_size.x) * size_t(m_size.y), float4(0.f));
  return aov.framebuffer && aov.resolved;
}

void Frame::deleteAOV(AOV &aov)
{
  auto *state = deviceState();
  state->deleteLater(aov.framebuffer, DeleteOrder::FRAMEBUFFER);
  state->deleteLater(aov.resolved, DeleteOrder::FRAMEBUFFER);
  aov.framebuffer = nullptr;
  aov.resolved    = nullptr;
  aov.data.clear();
}

void Frame::resolveAOV(AOV &aov, bool isColor)
{
  auto *state = deviceState();
  if (!aov.framebuffer || !aov.resolved)
    return;
  // the display gamma is never applied: the color channel of ANARI is linear
  CHECK(rprContextResolveFrameBuffer(state->context, aov.framebuffer, aov.resolved, !isColor))
  CHECK(rprFrameBufferGetInfo(
      aov.resolved, RPR_FRAMEBUFFER_DATA, aov.data.size() * sizeof(float4), aov.data.data(), nullptr))
}

bool Frame::createFramebuffers()
{
  const bool needsDepth = m_depth_type != ANARI_UNKNOWN;
  if (m_color.framebuffer && m_framebuffer_size == m_size && m_framebuffer_has_depth == needsDepth)
    return true;

  deleteFramebuffers();

  bool success = createAOV(m_color) && createAOV(m_opacity);
  if (needsDepth)
    success = success && createAOV(m_position);
  if (!success)
  {
    deleteFramebuffers();
    return false;
  }

  m_framebuffer_size      = m_size;
  m_framebuffer_has_depth = needsDepth;
  m_frame_changed         = true;
  return true;
}

void Frame::deleteFramebuffers()
{
  deleteAOV(m_color);
  deleteAOV(m_opacity);
  deleteAOV(m_position);
  m_framebuffer_size = uint2(0u);
}

void Frame::resolve(bool visibleEnvironment)
{
  resolveAOV(m_color, true);
  resolveAOV(m_opacity, false);
  resolveAOV(m_position, false);

  const size_t width  = m_size.x;
  const size_t height = m_size.y;

  const bool hasColor = !m_pixel_buffer.empty();
  const bool hasDepth = !m_depth_buffer.empty() && !m_position.data.empty();

  for (size_t y = 0; y < height; y++)
  {
    const size_t srcRow = FLIP_FRAMEBUFFER_ROWS ? height - 1 - y : y;
    for (size_t x = 0; x < width; x++)
    {
      const size_t src = srcRow * width + x;
      const size_t dst = y * width + x;

      // part of the pixel which is covered by the objects of the scene
      const float opacity = std::clamp(m_opacity.data[src].x, 0.f, 1.f);

      if (hasColor)
      {
        // the color includes the background, which is either the environment or the image of the scene
        float4 color = m_color.data[src];
        if (visibleEnvironment)
        {
          // a visible environment light replaces the background of the renderer
          color.w = 1.f;
        } else
        {
          const float4 background =
              m_renderer->background((float(x) + 0.5f) / float(width), (float(y) + 0.5f) / float(height));
          color.w = opacity + (1.f - opacity) * background.w;
        }
        color.x = std::max(color.x, 0.f);
        color.y = std::max(color.y, 0.f);
        color.z = std::max(color.z, 0.f);

        switch (m_color_type)
        {
        case ANARI_UFIXED8_VEC4: {
          const uint32_t c = cvt_color_to_uint32(color);
          std::memcpy(m_pixel_buffer.data() + dst * sizeof(c), &c, sizeof(c));
          break;
        }
        case ANARI_UFIXED8_RGBA_SRGB: {
          const uint32_t c = cvt_color_to_uint32_srgb(color);
          std::memcpy(m_pixel_buffer.data() + dst * sizeof(c), &c, sizeof(c));
          break;
        }
        case ANARI_FLOAT32_VEC4: {
          std::memcpy(m_pixel_buffer.data() + dst * sizeof(color), &color, sizeof(color));
          break;
        }
        default:
          break;
        }
      }

      if (hasDepth)
      {
        const float4 &position = m_position.data[src];
        m_depth_buffer[dst]    = opacity > 0.f ? m_camera->depth(float3(position.x, position.y, position.z))
                                               : std::numeric_limits<float>::infinity();
      }
    }
  }
}

void *Frame::map(std::string_view channel, uint32_t *width, uint32_t *height, ANARIDataType *pixelType)
{
  wait();

  *width  = m_size.x;
  *height = m_size.y;

  if (channel == "channel.color" && !m_pixel_buffer.empty())
  {
    *pixelType = m_color_type;
    return m_pixel_buffer.data();
  } else if (channel == "channel.depth" && !m_depth_buffer.empty())
  {
    *pixelType = m_depth_type;
    return m_depth_buffer.data();
  } else
  {
    *width     = 0;
    *height    = 0;
    *pixelType = ANARI_UNKNOWN;
    return nullptr;
  }
}

void Frame::unmap(std::string_view channel)
{
  // no-op
}

int Frame::frameReady(ANARIWaitMask m)
{
  if (m == ANARI_NO_WAIT)
    return ready();
  else
  {
    wait();
    return 1;
  }
}

void Frame::discard()
{
  auto *state = deviceState();
  if (state->context && m_future.valid() && !helium::tasking::isReady(m_future))
    rprContextAbortRender(state->context);
}

bool Frame::ready() const
{
  return completingOnThisThread() || !m_future.valid() || helium::tasking::isReady(m_future);
}

void Frame::wait()
{
  // A completion callback runs inside the job m_future tracks; that job has
  // finished rendering, and waiting on it from there would never return.
  if (completingOnThisThread())
    return;
  if (m_future.valid())
  {
    m_future.get();
    this->refDec(helium::RefType::INTERNAL);
  }
}

void Frame::waitOnOutstandingWorkIfNeeded()
{
  auto *state = deviceState();
  if (!state->taskQueue.onWorkerThread())
    wait();
}

} // namespace rpr

ANARI_TYPEFOR_DEFINITION(rpr::Frame *);

} // namespace anari
