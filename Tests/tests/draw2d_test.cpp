#include <Tempest/VulkanApi>
#include <Tempest/MetalApi>

#include <Tempest/Device>
#include <Tempest/Painter>
#include <Tempest/TextureAtlas>
#include <Tempest/VectorImage>
#include <Tempest/Event>
#include <Tempest/Log>
#include <Tempest/Except>

#include <gtest/gtest.h>
#include <gmock/gmock-matchers.h>

using namespace testing;
using namespace Tempest;

TEST(main,Draw2d) {
  try {
#if defined(__OSX__)
    MetalApi     api;
#else
    VulkanApi    api;
#endif
    Device       device(api);
    TextureAtlas atlas(device);

    VectorImage  img;

    PaintEvent ev(img,atlas,64,64);
    Painter p(ev);

    p.drawRect(0,0,32,32);
    }
  catch(std::system_error& e) {
    if(e.code()==Tempest::GraphicsErrc::NoDevice)
      Log::d("Skipping graphics testcase: ", e.what()); else
      throw;
    }
  }
