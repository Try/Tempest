#include <Tempest/VulkanApi>
#include <Tempest/Device>
#include <Tempest/Painter>
#include <Tempest/TextureAtlas>
#include <Tempest/VectorImage>
#include <Tempest/Event>

#include <gtest/gtest.h>
#include <gmock/gmock-matchers.h>

using namespace testing;
using namespace Tempest;

TEST(main,Draw2d) {
  VulkanApi    api;
  Device       device(api,nullptr);
  TextureAtlas atlas(device);

  VectorImage  img;

  PaintEvent ev(img,atlas,64,64);
  Painter p(ev);

  p.drawRect(0,0,32,32);
  }
