#include "renderdoc.h"

#include <Tempest/Platform>
#include <stdexcept>

#include "renderdoc_app.h"

#ifdef __WINDOWS__
#include <windows.h>
#endif

static RENDERDOC_API_1_4_2* rdocApi = nullptr;

RenderDoc::RenderDoc() {
#ifdef __WINDOWS__
  if(HMODULE mod = GetModuleHandleA("renderdoc.dll")) {
    pRENDERDOC_GetAPI RENDERDOC_GetAPI = (pRENDERDOC_GetAPI)GetProcAddress(mod, "RENDERDOC_GetAPI");
    int ret = RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_4_2, (void **)&rdocApi);
    if(ret!=1) {
      throw std::runtime_error("unable to initialize renderdoc");
      }
    }
#endif
  }

RenderDoc::~RenderDoc() {
  }

void RenderDoc::start() {
  // To start a frame capture, call StartFrameCapture.
  // You can specify NULL, NULL for the device to capture on if you have only one device and
  // either no windows at all or only one window, and it will capture from that device.
  // See the documentation below for a longer explanation
  if(rdocApi!=nullptr)
    rdocApi->StartFrameCapture(nullptr,nullptr);
  }

void RenderDoc::stop() {
  if(rdocApi!=nullptr)
    rdocApi->EndFrameCapture(nullptr, nullptr);
  }
