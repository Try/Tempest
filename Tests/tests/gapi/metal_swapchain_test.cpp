#include <Tempest/Application>
#include <Tempest/Window>
#include <Tempest/MetalApi>
#include <Tempest/Device>
#include <Tempest/Fence>
#include <Tempest/Log>

#include <gtest/gtest.h>

#if defined(__OSX__)
using namespace Tempest;

namespace {
class TestWindow : public Window {
  public:
    using Window::hwnd;
  };
}

TEST(MetalApi,DynamicSwapchain) {
  try {
    Application app;
    TestWindow window;
    MetalApi api;
    Device device(api);
    auto swapchain = device.swapchain(window.hwnd());
    CommandBuffer command;

    for(int i=0; i<12; ++i) {
      auto& image = swapchain.acquire();
      EXPECT_FALSE(image.isEmpty());
      EXPECT_EQ(image.w(),int(swapchain.w()));
      EXPECT_EQ(image.h(),int(swapchain.h()));
      EXPECT_THROW(textureCast<Texture2d&>(image),BadTextureCastException);

      EXPECT_EQ(&image,&swapchain.acquire());

      {
        auto encoder = command.startEncoding(device);
        encoder.setFramebuffer({{image,Vec4(0.25f,0.5f,0.75f,1.f),Preserve}});
        encoder.setFramebuffer({});
        encoder.setFramebuffer({{image,Preserve,Preserve}});
        }
      auto fence = device.submit(command);
      if(i==5) {
        fence.wait();
        swapchain.reset();
        continue;
        }
      // An early return after submitting must not acquire another drawable.
      EXPECT_EQ(&image,&swapchain.acquire());
      if(i%3==0) {
        fence.wait();
        continue;
        }
      device.present(swapchain);
      fence.wait();
      }

    {
      const auto size = swapchain.acquire().size();
      auto moved = std::move(swapchain);
      EXPECT_EQ(moved.acquire().size(),size);
      swapchain = std::move(moved);
      EXPECT_EQ(swapchain.acquire().size(),size);
      swapchain.reset();
      }

    // Keep the indexed API working while other backends migrate.
    {
      auto encoder = command.startEncoding(device);
      auto& image = swapchain[swapchain.currentImage()];
      EXPECT_FALSE(image.isEmpty());
      encoder.setFramebuffer({{image,Vec4(0),Preserve}});
      }
    auto fence = device.submit(command);
    device.present(swapchain);
    fence.wait();
    device.waitIdle();

    {
      auto encoder = command.startEncoding(device);
      Attachment empty;
      EXPECT_THROW(encoder.setFramebuffer({{empty,Vec4(0),Preserve}}),std::system_error);
      }
    }
  catch(const std::system_error& e) {
    if(e.code()!=GraphicsErrc::NoDevice)
      throw;
    Log::d("Skipping graphics testcase: ",e.what());
    }
  }
#endif
