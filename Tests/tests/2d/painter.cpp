#include <Tempest/VulkanApi>
#include <Tempest/DirectX12Api>
#include <Tempest/MetalApi>

#include <Tempest/Except>
#include <Tempest/Device>
#include <Tempest/Fence>
#include <Tempest/Pixmap>
#include <Tempest/Log>
#include <Tempest/VectorImage>
#include <Tempest/Event>
#include <Tempest/Painter>

#include <gtest/gtest.h>
#include <gmock/gmock-matchers.h>

using namespace Tempest;

class PainterTest : public ::testing::Test {
  protected:
    PainterTest(){ throw std::runtime_error(""); }
    void SetUp()    override {}
    void TearDown() override {}

    void draw(Attachment& fbo, const VectorImage::Mesh& mesh) {
      auto cmd = device.commandBuffer();
      {
        auto enc = cmd.startEncoding(device);
        enc.setFramebuffer({{fbo,Vec4(0,0,0,1),Tempest::Preserve}});
        mesh.draw(enc);
      }

      auto sync = device.fence();
      device.submit(cmd,sync);
      sync.wait();
      }

    void logImage(const Attachment& fbo) {
      auto name  = ::testing::UnitTest::GetInstance()->current_test_info()->name();
      auto tcase = ::testing::UnitTest::GetInstance()->current_test_info()->test_case_name();
      char buf[256] = {};
      std::snprintf(buf,sizeof(buf),"%s-%s.png",tcase,name);

      auto pm = device.readPixels(fbo);
      pm.save(buf);
      }

#if defined(__OSX__)
    MetalApi     api{ApiFlags::Validation};
#else
    VulkanApi    api{ApiFlags::Validation};
#endif
    Device       device{api};
    TextureAtlas atlas{device};
  };

TEST_F(PainterTest, DISABLED_Quad)
{
  VectorImage       img;
  VectorImage::Mesh imgMesh;

  auto tex = device.texture("data/img/tst-dxt5.dds");
  auto fbo = device.attachment(TextureFormat::RGBA8,512,512);
  {
  PaintEvent e(img,atlas,fbo.w(),fbo.h());
  Painter    p(e);

  p.setBrush(Color(0,0,1,1));
  p.drawRect(32,32,128,128);

  p.setBrush(Color(0,1,0,1));
  p.drawRect(256,32,128,128);

  p.setBrush(Color(1,0,0,1));
  p.drawRect(32,256,128,128);

  p.setBrush(Brush(tex,Color(1,1,0,1)));
  p.drawRect(256,256,128,128,
             0,0,tex.w(),tex.h());
  }

  imgMesh.update(device,img);
  draw(fbo, imgMesh);
  logImage(fbo);
}

TEST_F(PainterTest, DISABLED_Line)
{
  VectorImage       img;
  VectorImage::Mesh imgMesh;

  auto fbo = device.attachment(TextureFormat::RGBA8,512,512);
  {
  PaintEvent e(img,atlas,fbo.w(),fbo.h());
  Painter    p(e);

  for(int i=1; i<=15; ++i) {
    p.setPen(Pen(Color(1,1,1,1),Painter::Alpha,float(i)));

    p.drawLine(20, 32*i,    20+32, 32*i+32);
    p.drawLine(70, 32*i+15, 70+32, 32*i+15);
    }

  for(int i=1; i<=15; ++i) {
    p.setPen(Pen(Color(1,1,1,1),Painter::Alpha,float(i)));
    p.drawLine(128+i*20, 32+400, 128+20+i*20, 32);
    }
  }

  imgMesh.update(device,img);
  draw(fbo, imgMesh);
  logImage(fbo);
}
