#pragma once


#include <Tempest/Device>
#include <Tempest/Except>
#include <Tempest/Fence>
#include <Tempest/Pixmap>
#include <Tempest/Log>
#include <Tempest/Matrix4x4>
#include <Tempest/Vec>

#include <gtest/gtest.h>
#include <gmock/gmock-matchers.h>

#include "utils/imagevalidator.h"

namespace GapiTestSync {

struct Vertex {
  float x,y;
  };

static const Vertex        vboData[3] = {{-1,-1},{1,-1},{1,1}};
static const Tempest::Vec3 vboData3[3] = {{-1,-1,0},{1,-1,0},{1,1,0}};
static const uint16_t      iboData[3] = {0,1,2};

template<class GraphicsApi>
void DispathToDraw(const char* outImage) {
  using namespace Tempest;

  try {
    GraphicsApi api{ApiFlags::Validation};
    Device      device(api);

    auto vbo    = device.vbo(vboData,3);
    auto ibo    = device.ibo(iboData,3);
    auto ssbo   = device.ssbo(nullptr, sizeof(Vec4)*4);
    auto tex    = device.attachment(TextureFormat::RGBA8,4,4);

    auto cs     = device.shader("shader/fillbuf.comp.sprv");
    auto psoC   = device.pipeline(cs);

    auto vs     = device.shader("shader/simple_test.vert.sprv");
    auto fs     = device.shader("shader/comp_test.frag.sprv");
    auto psoG   = device.pipeline<Vertex>(Topology::Triangles,RenderState(),vs,fs);

    auto uboCs  = device.descriptors(psoC.layout());
    uboCs.set(0,ssbo);

    auto uboFs  = device.descriptors(psoG.layout());
    uboFs.set(0,ssbo);

    auto cmd = device.commandBuffer();
    {
      auto enc = cmd.startEncoding(device);
      enc.setUniforms(psoC,uboCs);
      enc.dispatch(4,1,1);

      enc.setFramebuffer({{tex,Vec4(0,0,0,0),Tempest::Preserve}});
      enc.setUniforms(psoG,uboFs);
      enc.draw(vbo,ibo);
    }

    auto sync = device.fence();
    device.submit(cmd,sync);
    sync.wait();

    auto pm = device.readPixels(tex);
    pm.save(outImage);
    }
  catch(std::system_error& e) {
    if(e.code()==Tempest::GraphicsErrc::NoDevice)
      Log::d("Skipping graphics testcase: ", e.what()); else
      throw;
    }
  }

template<class GraphicsApi>
void DrawToDispath() {
  using namespace Tempest;

  try {
    GraphicsApi api{ApiFlags::Validation};
    Device      device(api);

    auto vbo    = device.vbo(vboData,3);
    auto ibo    = device.ibo(iboData,3);
    auto tex    = device.attachment(TextureFormat::RGBA8,4,4);
    auto ssbo   = device.ssbo(nullptr, sizeof(Vec4)*tex.w()*tex.h());

    auto cs     = device.shader("shader/img2buf.comp.sprv");
    auto psoC   = device.pipeline(cs);

    auto vs     = device.shader("shader/simple_test.vert.sprv");
    auto fs     = device.shader("shader/simple_test.frag.sprv");
    auto psoG   = device.pipeline<Vertex>(Topology::Triangles,RenderState(),vs,fs);

    auto uboCs  = device.descriptors(psoC.layout());
    uboCs.set(0,tex);
    uboCs.set(1,ssbo);

    auto cmd = device.commandBuffer();
    {
      auto enc = cmd.startEncoding(device);
      enc.setFramebuffer({{tex,Vec4(0,0,1,1),Tempest::Preserve}});
      enc.setUniforms(psoG);
      enc.draw(vbo,ibo);

      enc.setFramebuffer({});
      enc.setUniforms(psoC,uboCs);
      enc.dispatch(tex.w(),tex.h(),1);
    }

    auto sync = device.fence();
    device.submit(cmd,sync);
    sync.wait();

    std::vector<Vec4> pm(tex.w()*tex.h());
    device.readBytes(ssbo,pm.data(),pm.size()*sizeof(pm[0]));

    ImageValidator val(pm,tex.w());
    for(int32_t y=0; y<tex.h(); ++y)
      for(int32_t x=0; x<tex.w(); ++x) {
        auto pix = val.at(x,y);
        auto ref = ImageValidator::Pixel();
        if(x<y) {
          ref.x[0] = 0;
          ref.x[1] = 0;
          ref.x[2] = 1;
          ref.x[3] = 1;
          } else {
          const float u = (float(x)+0.5f)/float(tex.w());
          const float v = (float(y)+0.5f)/float(tex.h());
          ref.x[0] = u;
          ref.x[1] = v;
          ref.x[2] = 0;
          ref.x[3] = 1;
          }

        for(uint32_t c=0; c<4; ++c)
          ASSERT_NEAR(pix.x[c],ref.x[c],0.01f);
        }
    }
  catch(std::system_error& e) {
    if(e.code()==Tempest::GraphicsErrc::NoDevice)
      Log::d("Skipping graphics testcase: ", e.what()); else
      throw;
    }
  }

}
