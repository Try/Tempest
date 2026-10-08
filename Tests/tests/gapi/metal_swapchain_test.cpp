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
      auto image = swapchain.next();
      EXPECT_FALSE(image.isEmpty());
      EXPECT_EQ(image.w(),int(swapchain.w()));
      EXPECT_EQ(image.h(),int(swapchain.h()));
      EXPECT_THROW(textureCast<Texture2d&>(image),BadTextureCastException);

      Attachment moved(std::move(image));
      EXPECT_TRUE(image.isEmpty());
      EXPECT_EQ(image.w(),0);
      image = std::move(moved);
      EXPECT_TRUE(moved.isEmpty());

      {
        auto encoder = command.startEncoding(device);
        encoder.setFramebuffer({{image,Vec4(0.25f,0.5f,0.75f,1.f),Preserve}});
        encoder.setFramebuffer({});
        encoder.setFramebuffer({{image,Preserve,Preserve}});
        }
      const bool discard = i%3==0;
      if(discard)
        image = Attachment();
      auto fence = device.submit(command);
      if(i==5) {
        const auto size = image.size();
        swapchain.reset();
        EXPECT_EQ(image.size(),size);
        }
      if(!discard)
        device.present(std::move(image));
      EXPECT_TRUE(image.isEmpty());
      EXPECT_THROW(device.present(std::move(image)),std::system_error);
      fence.wait();
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

    auto offscreen = device.attachment(TextureFormat::RGBA8,16,16);
    EXPECT_THROW(device.present(std::move(offscreen)),std::system_error);
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
