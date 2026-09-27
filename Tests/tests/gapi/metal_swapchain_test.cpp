#include <Tempest/Application>
#include <Tempest/Window>
#include <Tempest/MetalApi>
#include <Tempest/Device>
#include <Tempest/Fence>
#include <Tempest/Log>

#include <gtest/gtest.h>
#include <stdexcept>

#if defined(__OSX__)
using namespace Tempest;

namespace {
class TestWindow : public Window {
  public:
    using Window::hwnd;
  };
}

TEST(MetalApi,SwapchainOptions) {
  try {
    Application app;
    TestWindow window;
    MetalApi api;
    Device device(api);

    for(auto count:{0u,2u,3u}) {
      for(auto mode:{Swapchain::RenderMode::Copy,Swapchain::RenderMode::Direct}) {
        Swapchain::Options options;
        options.bufferCount = count;
        options.renderMode  = mode;
        auto swapchain = device.swapchain(window.hwnd(),options);
        EXPECT_EQ(swapchain.imageCount(),count==0 ? 3u : count);
        CommandBuffer command;
        for(int i=0; i<12; ++i) {
          if(i==6)
            swapchain.reset();
          {
            auto encoder = command.startEncoding(device);
            auto& image = swapchain[swapchain.currentImage()];
            encoder.setFramebuffer({{image,Vec4(0.25f,0.5f,0.75f,1.f),Preserve}});
            encoder.setFramebuffer({});
            encoder.setFramebuffer({{image,Preserve,Preserve}});
            }
          auto fence = device.submit(command);
          device.present(swapchain);
          fence.wait();
          }
        device.waitIdle();
        }
      }

    for(auto count:{1u,4u,uint32_t(-1)}) {
      Swapchain::Options options;
      options.bufferCount = count;
      EXPECT_THROW(device.swapchain(window.hwnd(),options),std::invalid_argument);
      }
    }
  catch(const std::system_error& e) {
    if(e.code()!=GraphicsErrc::NoDevice)
      throw;
    Log::d("Skipping graphics testcase: ",e.what());
    }
  }
#endif
