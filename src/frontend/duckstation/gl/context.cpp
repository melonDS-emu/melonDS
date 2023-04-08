#include "context.h"
#include "../log.h"
#include "loader.h"
#include <cstdlib>
#include <cstring>
#ifdef __APPLE__
#include <stdlib.h>
#else
#include <malloc.h>
#endif
Log_SetChannel(GL::Context);

#if defined(_WIN32)
#include "context_wgl.h"
#elif defined(__APPLE__)
#include "context_agl.h"
#else
#ifdef WAYLAND_ENABLED
#include "context_egl_wayland.h"
#endif
#include "context_egl_x11.h"
#include "context_glx.h"
#endif

namespace GL {

static bool ShouldPreferESContext()
{
#ifndef _MSC_VER
  const char* value = std::getenv("PREFER_GLES_CONTEXT");
  return (value && strcmp(value, "1") == 0);
#else
  char buffer[2] = {};
  size_t buffer_size = sizeof(buffer);
  getenv_s(&buffer_size, buffer, "PREFER_GLES_CONTEXT");
  return (std::strcmp(buffer, "1") == 0);
#endif
}

Context::Context(const WindowInfo& wi) : m_wi(wi) {}

Context::~Context() = default;

std::vector<Context::FullscreenModeInfo> Context::EnumerateFullscreenModes()
{
  return {};
}

std::unique_ptr<GL::Context> Context::Create(const WindowInfo& wi, const Version* versions_to_try,
                                             size_t num_versions_to_try)
{
  if (ShouldPreferESContext())
  {
    // move ES versions to the front
    Version* new_versions_to_try = static_cast<Version*>(alloca(sizeof(Version) * num_versions_to_try));
    size_t count = 0;
    for (size_t i = 0; i < num_versions_to_try; i++)
    {
      if (versions_to_try[i].profile == Profile::ES)
        new_versions_to_try[count++] = versions_to_try[i];
    }
    for (size_t i = 0; i < num_versions_to_try; i++)
    {
      if (versions_to_try[i].profile != Profile::ES)
        new_versions_to_try[count++] = versions_to_try[i];
    }
    versions_to_try = new_versions_to_try;
  }

  std::unique_ptr<Context> context;
#if defined(_WIN32)
  context = ContextWGL::Create(wi, versions_to_try, num_versions_to_try);
#elif defined(__APPLE__)
  context = ContextAGL::Create(wi, versions_to_try, num_versions_to_try);
#else
  if (wi.type == WindowInfo::Type::X11)
  {
    const char* use_egl_x11 = std::getenv("USE_EGL_X11");
    if (use_egl_x11 && std::strcmp(use_egl_x11, "1") == 0)
      context = ContextEGLX11::Create(wi, versions_to_try, num_versions_to_try);
    else
      context = ContextGLX::Create(wi, versions_to_try, num_versions_to_try);
  }

#ifdef WAYLAND_ENABLED
  if (wi.type == WindowInfo::Type::Wayland)
    context = ContextEGLWayland::Create(wi, versions_to_try, num_versions_to_try);
#endif
#endif

  if (!context)
    return nullptr;

  Log_InfoPrintf("Created a %s context", context->IsGLES() ? "OpenGL ES" : "OpenGL");

  // TODO: Not thread-safe.
  static Context* context_being_created;
  context_being_created = context.get();

  if (!gladLoadGLLoader([](const char* name) { return context_being_created->GetProcAddress(name); }))
  {
    Log_ErrorPrintf("Failed to load GL functions for GLAD");
    return nullptr;
  }

  const char* gl_vendor = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
  const char* gl_renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
  const char* gl_version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
  const char* gl_shading_language_version = reinterpret_cast<const char*>(glGetString(GL_SHADING_LANGUAGE_VERSION));
  Log_InfoPrintf("GL_VENDOR: %s", gl_vendor);
  Log_InfoPrintf("GL_RENDERER: %s", gl_renderer);
  Log_InfoPrintf("GL_VERSION: %s", gl_version);
  Log_InfoPrintf("GL_SHADING_LANGUAGE_VERSION: %s", gl_shading_language_version);

  return context;
}

const std::array<Context::Version, 11>& Context::GetAllDesktopVersionsList()
{
  static constexpr std::array<Version, 11> vlist = {{{Profile::Core, 4, 6},
                                                     {Profile::Core, 4, 5},
                                                     {Profile::Core, 4, 4},
                                                     {Profile::Core, 4, 3},
                                                     {Profile::Core, 4, 2},
                                                     {Profile::Core, 4, 1},
                                                     {Profile::Core, 4, 0},
                                                     {Profile::Core, 3, 3},
                                                     {Profile::Core, 3, 2},
                                                     {Profile::Core, 3, 1},
                                                     {Profile::Core, 3, 0}}};
  return vlist;
}

const std::array<Context::Version, 12>& Context::GetAllDesktopVersionsListWithFallback()
{
  static constexpr std::array<Version, 12> vlist = {{{Profile::Core, 4, 6},
                                                     {Profile::Core, 4, 5},
                                                     {Profile::Core, 4, 4},
                                                     {Profile::Core, 4, 3},
                                                     {Profile::Core, 4, 2},
                                                     {Profile::Core, 4, 1},
                                                     {Profile::Core, 4, 0},
                                                     {Profile::Core, 3, 3},
                                                     {Profile::Core, 3, 2},
                                                     {Profile::Core, 3, 1},
                                                     {Profile::Core, 3, 0},
                                                     {Profile::NoProfile, 0, 0}}};
  return vlist;
}

const std::array<Context::Version, 4>& Context::GetAllESVersionsList()
{
  static constexpr std::array<Version, 4> vlist = {
    {{Profile::ES, 3, 2}, {Profile::ES, 3, 1}, {Profile::ES, 3, 0}, {Profile::ES, 2, 0}}};
  return vlist;
}

const std::array<Context::Version, 16>& Context::GetAllVersionsList()
{
  static constexpr std::array<Version, 16> vlist = {{{Profile::Core, 4, 6},
                                                     {Profile::Core, 4, 5},
                                                     {Profile::Core, 4, 4},
                                                     {Profile::Core, 4, 3},
                                                     {Profile::Core, 4, 2},
                                                     {Profile::Core, 4, 1},
                                                     {Profile::Core, 4, 0},
                                                     {Profile::Core, 3, 3},
                                                     {Profile::Core, 3, 2},
                                                     {Profile::Core, 3, 1},
                                                     {Profile::Core, 3, 0},
                                                     {Profile::ES, 3, 2},
                                                     {Profile::ES, 3, 1},
                                                     {Profile::ES, 3, 0},
                                                     {Profile::ES, 2, 0},
                                                     {Profile::NoProfile, 0, 0}}};
  return vlist;
}

} // namespace GL
