#pragma once

#include <Tempest/Device>
#include <Tempest/Except>
#include <Tempest/Fence>
#include <Tempest/Pixmap>
#include <Tempest/Log>
#include <Tempest/Vec>

#include <gtest/gtest.h>
#include <gmock/gmock-matchers.h>

namespace GapiTestCommon {

struct Vertex {
  float x,y;
  };

static const Vertex   vboData[3] = {{-1,-1},{1,-1},{1,1}};
static const uint16_t iboData[3] = {0,1,2};

template<class GraphicsApi>
void init() {
  using namespace Tempest;

  try {
    GraphicsApi api{ApiFlags::Validation};
    (void)api;
    }
  catch(std::system_error& e) {
    if(e.code()==Tempest::GraphicsErrc::NoDevice)
      Log::d("Skipping graphics testcase: ", e.what()); else
      throw;
    }
  }

template<class GraphicsApi>
void vbo() {
  using namespace Tempest;

  try {
    GraphicsApi api{ApiFlags::Validation};
    Device      device(api);

    auto vbo = device.vbo(vboData,3);
    auto ibo = device.ibo(iboData,3);
    }
  catch(std::system_error& e) {
    if(e.code()==Tempest::GraphicsErrc::NoDevice)
      Log::d("Skipping graphics testcase: ", e.what()); else
      throw;
    }
  }

template<class GraphicsApi>
void shader() {
  using namespace Tempest;

  try {
    GraphicsApi api{ApiFlags::Validation};
    Device      device(api);

    auto vert = device.loadShader("shader/simple_test.vert.sprv");
    auto frag = device.loadShader("shader/simple_test.frag.sprv");
    }
  catch(std::system_error& e) {
    if(e.code()==Tempest::GraphicsErrc::NoDevice)
      Log::d("Skipping graphics testcase: ", e.what()); else
      throw;
    }
  }

template<class GraphicsApi>
void pso() {
  using namespace Tempest;

  try {
    GraphicsApi api{ApiFlags::Validation};
    Device      device(api);

    auto vert = device.loadShader("shader/simple_test.vert.sprv");
    auto frag = device.loadShader("shader/simple_test.frag.sprv");
    auto pso  = device.pipeline<Vertex>(Topology::Triangles,RenderState(),vert,frag);
    }
  catch(std::system_error& e) {
    if(e.code()==Tempest::GraphicsErrc::NoDevice)
      Log::d("Skipping graphics testcase: ", e.what()); else
      throw;
    }
  }

template<class GraphicsApi>
void psoTess() {
  using namespace Tempest;

  try {
    GraphicsApi api{ApiFlags::Validation};
    Device      device(api);

    auto vert = device.loadShader("shader/tess.vert.sprv");
    auto frag = device.loadShader("shader/tess.frag.sprv");
    auto tese = device.loadShader("shader/tess.tese.sprv");
    auto tesc = device.loadShader("shader/tess.tesc.sprv");
    auto pso  = device.pipeline<Vertex>(Topology::Triangles,RenderState(),vert,tesc,tese,frag);
    }
  catch(std::system_error& e) {
    if(e.code()==Tempest::GraphicsErrc::NoDevice)
      Log::d("Skipping graphics testcase: ", e.what()); else
      throw;
    }
  }

template<class GraphicsApi>
void fbo(const char* outImg) {
  using namespace Tempest;

  try {
    GraphicsApi api{ApiFlags::Validation};
    Device      device(api);

    auto tex = device.attachment(TextureFormat::RGBA8,128,128);
    auto fbo = device.frameBuffer(tex);
    auto rp  = device.pass(FboMode(FboMode::PreserveOut,Color(0.f,0.f,1.f)));

    auto cmd = device.commandBuffer();
    {
      auto enc = cmd.startEncoding(device);
      enc.setFramebuffer(fbo,rp);
    }

    auto sync = device.fence();
    device.submit(cmd,sync);
    sync.wait();

    auto pm = device.readPixels(tex);
    pm.save(outImg);
    }
  catch(std::system_error& e) {
    if(e.code()==Tempest::GraphicsErrc::NoDevice)
      Log::d("Skipping graphics testcase: ", e.what()); else
      throw;
    }
  }

template<class GraphicsApi, Tempest::TextureFormat format>
void draw(const char* outImage) {
  using namespace Tempest;

  try {
    GraphicsApi api{ApiFlags::Validation};
    Device      device(api);

    auto vbo  = device.vbo(vboData,3);
    auto ibo  = device.ibo(iboData,3);

    auto vert = device.loadShader("shader/simple_test.vert.sprv");
    auto frag = device.loadShader("shader/simple_test.frag.sprv");
    auto pso  = device.pipeline<Vertex>(Topology::Triangles,RenderState(),vert,frag);

    auto tex  = device.attachment(format,128,128);
    auto fbo  = device.frameBuffer(tex);
    auto rp   = device.pass(FboMode(FboMode::PreserveOut,Color(0.f,0.f,1.f)));

    auto cmd  = device.commandBuffer();
    {
      auto enc = cmd.startEncoding(device);
      enc.setFramebuffer(fbo,rp);
      enc.setUniforms(pso);
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
void ssboDispath() {
  using namespace Tempest;

  try {
    GraphicsApi api{ApiFlags::Validation};
    Device      device(api);

    Vec4 inputCpu[3] = {Vec4(0,1,2,3),Vec4(4,5,6,7),Vec4(8,9,10,11)};

    auto input  = device.ssbo<Tempest::Vec4>(inputCpu,3);
    auto output = device.ssbo<Tempest::Vec4>(nullptr, 3);

    auto cs     = device.loadShader("shader/simple_test.comp.sprv");
    auto pso    = device.pipeline(cs);

    auto ubo = device.uniforms(pso.layout());
    ubo.set(0,input);
    ubo.set(1,output);

    auto cmd = device.commandBuffer();
    {
      auto enc = cmd.startEncoding(device);
      enc.setUniforms(pso,ubo);
      enc.dispatch(3,1,1);
    }

    auto sync = device.fence();
    device.submit(cmd,sync);
    sync.wait();

    Vec4 outputCpu[3] = {};
    device.readBytes(output,outputCpu,3);

    for(size_t i=0; i<3; ++i)
      EXPECT_EQ(outputCpu[i],inputCpu[i]);
    }
  catch(std::system_error& e) {
    if(e.code()==Tempest::GraphicsErrc::NoDevice)
      Log::d("Skipping graphics testcase: ", e.what()); else
      throw;
    }
  }

template<class GraphicsApi>
void imageCompute(const char* outImage) {
  using namespace Tempest;

  try {
    GraphicsApi api{ApiFlags::Validation};
    Device      device(api);

    auto img = device.image2d(TextureFormat::RGBA8,128,128,false);

    auto cs  = device.loadShader("shader/image_store_test.comp.sprv");
    auto pso = device.pipeline(cs);

    auto ubo = device.uniforms(pso.layout());
    ubo.set(0,img);

    auto cmd = device.commandBuffer();
    {
      auto enc = cmd.startEncoding(device);
      enc.setUniforms(pso,ubo);
      enc.dispatch(img.w(),img.h(),1);
    }

    auto sync = device.fence();
    device.submit(cmd,sync);
    sync.wait();

    auto pm = device.readPixels(img);
    pm.save(outImage);
    }
  catch(std::system_error& e) {
    if(e.code()==Tempest::GraphicsErrc::NoDevice)
      Log::d("Skipping graphics testcase: ", e.what()); else
      throw;
    }
  }

template<class GraphicsApi, Tempest::TextureFormat format>
void mipMaps(const char* outImage) {
  using namespace Tempest;

  try {
    GraphicsApi api{ApiFlags::Validation};
    Device      device(api);

    auto vbo  = device.vbo(vboData,3);
    auto ibo  = device.ibo(iboData,3);

    auto vert = device.loadShader("shader/simple_test.vert.sprv");
    auto frag = device.loadShader("shader/simple_test.frag.sprv");
    auto pso  = device.pipeline<Vertex>(Topology::Triangles,RenderState(),vert,frag);

    auto tex  = device.attachment(format,128,128,true);
    auto fbo  = device.frameBuffer(tex);
    auto rp   = device.pass(FboMode(FboMode::PreserveOut,Color(0.f,0.f,1.f)));
    auto sync = device.fence();

    auto cmd = device.commandBuffer();
    {
      auto enc = cmd.startEncoding(device);
      enc.setFramebuffer(fbo,rp);
      enc.setUniforms(pso);
      enc.draw(vbo,ibo);
      enc.setFramebuffer(nullptr);
      enc.generateMipmaps(tex);
    }
    device.submit(cmd,sync);
    sync.wait();

    auto pm = device.readPixels(tex,1);
    pm.save(outImage);
    }
  catch(std::system_error& e) {
    if(e.code()==Tempest::GraphicsErrc::NoDevice)
      Log::d("Skipping graphics testcase: ", e.what()); else
      throw;
    }
  }
}
