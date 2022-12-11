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

namespace GapiTestCommon {

struct Vertex {
  float x,y;
  };

static const Vertex        vboData[3] = {{-1,-1},{1,-1},{1,1}};
static const Tempest::Vec3 vboData3[3] = {{-1,-1,0},{1,-1,0},{1,1,0}};
static const uint16_t      iboData[3] = {0,1,2};

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
void Vbo() {
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
void VboInit() {
  using namespace Tempest;

  try {
    GraphicsApi api{ApiFlags::Validation};
    Device      device(api);

    auto vboD = device.vbo(BufferHeap::Device,  vboData,3);
    auto vboR = device.vbo(BufferHeap::Readback,vboData,3);
    auto vboU = device.vbo(BufferHeap::Upload,  vboData,3);
    EXPECT_EQ(vboD.size(),3);
    EXPECT_EQ(vboR.size(),3);
    EXPECT_EQ(vboU.size(),3);

    auto iboD = device.ibo(BufferHeap::Device,  iboData,3);
    auto iboR = device.ibo(BufferHeap::Readback,iboData,3);
    auto iboU = device.ibo(BufferHeap::Upload,  iboData,3);
    EXPECT_EQ(iboD.size(),3);
    EXPECT_EQ(iboR.size(),3);
    EXPECT_EQ(iboU.size(),3);

    auto uboD = device.ubo(BufferHeap::Device,  vboData,3);
    auto uboR = device.ubo(BufferHeap::Readback,vboData,3);
    auto uboU = device.ubo(BufferHeap::Upload,  vboData,3);
    EXPECT_EQ(uboD.size(),3);
    EXPECT_EQ(uboR.size(),3);
    EXPECT_EQ(uboU.size(),3);

    auto ssboD = device.ssbo(BufferHeap::Device,  vboData,sizeof(vboData));
    auto ssboR = device.ssbo(BufferHeap::Readback,vboData,sizeof(vboData));
    auto ssboU = device.ssbo(BufferHeap::Upload,  vboData,sizeof(vboData));
    EXPECT_EQ(ssboD.byteSize(),sizeof(vboData));
    EXPECT_EQ(ssboR.byteSize(),sizeof(vboData));
    EXPECT_EQ(ssboU.byteSize(),sizeof(vboData));
    }
  catch(std::system_error& e) {
    if(e.code()==Tempest::GraphicsErrc::NoDevice)
      Log::d("Skipping graphics testcase: ", e.what()); else
      throw;
    }
  }

template<class GraphicsApi>
void VboDyn() {
  using namespace Tempest;

  Vertex readback[3] = {};
  try {
    GraphicsApi api{ApiFlags::Validation};
    Device      device(api);

    auto vbo = device.vbo(vboData,3);

    Vertex   vboData2[3] = {vboData[0],{3,4},{5,6}};
    vbo.update(vboData2,1,2);

    auto ssbo = device.ssbo(BufferHeap::Upload,vboData,sizeof(vboData));
    ssbo.update(vboData2+1, 1*sizeof(vboData2[0]), 2*sizeof(vboData2[0]));

    device.readBytes(ssbo,readback,ssbo.byteSize());
    for(int i=0; i<3; ++i) {
      EXPECT_EQ(readback[i].x,vboData2[i].x);
      EXPECT_EQ(readback[i].y,vboData2[i].y);
      }
    }
  catch(std::system_error& e) {
    if(e.code()==Tempest::GraphicsErrc::NoDevice)
      Log::d("Skipping graphics testcase: ", e.what()); else
      throw;
    }
  }

template<class GraphicsApi, class T>
void SsboDyn() {
  using namespace Tempest;
  try {
    GraphicsApi api{ApiFlags::Validation};
    Device      device(api);

    const size_t eltCount = 4096+100;

    auto ssbo = device.ssbo(nullptr,eltCount*sizeof(T));
    for(size_t i=0; i<eltCount; ++i) {
      T val = T(i*i);
      ssbo.update(&val,i*sizeof(T),sizeof(T));
      }

    std::vector<T> data(eltCount);
    device.readBytes(ssbo,data.data(),data.size()*sizeof(T));

    size_t eqCount = 0;
    for(size_t i=0; i<eltCount; ++i) {
      T val = data[i];
      if(val==T(i*i))
        ++eqCount;
      }
    EXPECT_EQ(eltCount,eqCount);
    }
  catch(std::system_error& e) {
    if(e.code()==Tempest::GraphicsErrc::NoDevice)
      Log::d("Skipping graphics testcase: ", e.what()); else
      throw;
    }
  }

template<class GraphicsApi, Tempest::TextureFormat frm, class iType>
void SsboCopy() {
  using namespace Tempest;

  try {
    GraphicsApi api{ApiFlags::Validation};
    Device      device(api);

    iType   ref[4]   = {32,64,128,255};
    float   maxIType = float(iType(-1));
    auto    src      = device.attachment(frm,4,4);

    auto bpp  = Pixmap::bppForFormat  (Pixmap::toPixmapFormat(frm));
    auto ccnt = Pixmap::componentCount(Pixmap::toPixmapFormat(frm));
    auto dst  = device.ssbo(nullptr, src.w()*src.h()*bpp);

    auto cmd = device.commandBuffer();
    {
      auto enc = cmd.startEncoding(device);
      enc.setFramebuffer({{src,Vec4(ref[0]/maxIType,ref[1]/maxIType,ref[2]/maxIType,ref[3]/maxIType),Tempest::Preserve}});
      enc.setFramebuffer({});
      enc.copy(src,0,dst,0);
    }

    auto sync = device.fence();
    device.submit(cmd,sync);
    sync.wait();

    std::vector<iType> dstCpu(src.w()*src.h()*ccnt);
    device.readBytes(dst,dstCpu.data(),dstCpu.size()*sizeof(iType));
    for(size_t i=0; i<dstCpu.size(); i+=ccnt) {
      for(size_t b=0; b<ccnt; ++b){
        EXPECT_EQ(dstCpu[i+b],ref[b]);
        }
      }
    }
  catch(std::system_error& e) {
    if(e.code()==Tempest::GraphicsErrc::NoDevice)
      Log::d("Skipping graphics testcase: ", e.what()); else
      throw;
    }
  }

template<class GraphicsApi>
void Shader() {
  using namespace Tempest;

  try {
    GraphicsApi api{ApiFlags::Validation};
    Device      device(api);

    auto vert = device.shader("shader/simple_test.vert.sprv");
    auto frag = device.shader("shader/simple_test.frag.sprv");
    }
  catch(std::system_error& e) {
    if(e.code()==Tempest::GraphicsErrc::NoDevice)
      Log::d("Skipping graphics testcase: ", e.what()); else
      throw;
    }
  }

template<class GraphicsApi>
void Pso() {
  using namespace Tempest;

  try {
    GraphicsApi api{ApiFlags::Validation};
    Device      device(api);

    auto vert = device.shader("shader/simple_test.vert.sprv");
    auto frag = device.shader("shader/simple_test.frag.sprv");
    auto pso  = device.pipeline(Topology::Triangles,RenderState(),vert,frag);
    }
  catch(std::system_error& e) {
    if(e.code()==Tempest::GraphicsErrc::NoDevice)
      Log::d("Skipping graphics testcase: ", e.what()); else
      throw;
    }
  }

template<class GraphicsApi>
void PsoInconsistentVaryings() {
  using namespace Tempest;

  try {
    GraphicsApi api{ApiFlags::Validation};
    Device      device(api);

    auto vert = device.shader("shader/simple_test.vert.sprv");
    auto frag = device.shader("shader/varying_test.frag.sprv");
    auto pso  = device.pipeline(Topology::Triangles,RenderState(),vert,frag);

    auto tex = device.attachment(TextureFormat::RGBA8,128,128);
    auto cmd = device.commandBuffer();
    {
      auto enc = cmd.startEncoding(device);
      enc.setFramebuffer({{tex,Vec4(0,0,1,1),Tempest::Preserve}});
      enc.setUniforms(pso); // late linkpakage
    }

    }
  catch(std::system_error& e) {
    if(e.code()==Tempest::GraphicsErrc::NoDevice)
      Log::d("Skipping graphics testcase: ", e.what()); else
      throw;
    }
  }

template<class GraphicsApi>
void PsoTess() {
  using namespace Tempest;

  try {
    GraphicsApi api{ApiFlags::Validation};
    Device      device(api);

    auto tese = device.shader("shader/tess.tese.sprv");
    auto tesc = device.shader("shader/tess.tesc.sprv");

    auto vert = device.shader("shader/tess.vert.sprv");
    auto frag = device.shader("shader/tess.frag.sprv");
    auto pso  = device.pipeline(Topology::Triangles,RenderState(),vert,tesc,tese,frag);
    }
  catch(std::system_error& e) {
    if(e.code()==Tempest::GraphicsErrc::NoDevice)
      Log::d("Skipping graphics testcase: ", e.what()); else
      throw;
    }
  }

template<class GraphicsApi>
void TesselationBasic(const char* outImg) {
  using namespace Tempest;

  try {
    GraphicsApi api{ApiFlags::Validation};
    Device      device(api);

    auto vbo  = device.vbo<Vertex>({{-1, 1},{0,-1},{1,1}});

    auto tese = device.shader("shader/tess_basic.tese.sprv");
    auto tesc = device.shader("shader/tess_basic.tesc.sprv");

    auto vert = device.shader("shader/tess_basic.vert.sprv");
    auto frag = device.shader("shader/tess_basic.frag.sprv");
    auto pso  = device.pipeline(Topology::Triangles,RenderState(),vert,tesc,tese,frag);

    auto tex = device.attachment(TextureFormat::RGBA8,128,128);
    auto cmd = device.commandBuffer();
    {
      auto enc = cmd.startEncoding(device);
      enc.setFramebuffer({{tex,Vec4(0,0,0,1),Tempest::Preserve}});
      enc.setUniforms(pso);
      enc.draw(vbo);
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

template<class GraphicsApi>
void GeomBasic(const char* outImg) {
  using namespace Tempest;

  try {
    const char* devName = nullptr;
    GraphicsApi api{ApiFlags::Validation};
    auto dev = api.devices();
    for(auto& i:dev)
      if(i.geometryShader)
        devName = i.name;
    if(devName==nullptr)
      return;

    Device      device(api);

    auto vert = device.shader("shader/geom_basic.vert.sprv");
    auto geom = device.shader("shader/geom_basic.geom.sprv");
    auto frag = device.shader("shader/geom_basic.frag.sprv");
    auto pso  = device.pipeline(Topology::Points,RenderState(),vert,geom,frag);

    auto tex = device.attachment(TextureFormat::RGBA8,128,128);
    auto cmd = device.commandBuffer();
    {
      auto enc = cmd.startEncoding(device);
      enc.setFramebuffer({{tex,Vec4(0,0,1,1),Tempest::Preserve}});
      enc.setUniforms(pso);
      enc.draw(1);
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

template<class GraphicsApi>
void Fbo(const char* outImg) {
  using namespace Tempest;

  try {
    GraphicsApi api{ApiFlags::Validation};
    Device      device(api);

    auto tex = device.attachment(TextureFormat::RGBA8,128,128);

    auto cmd = device.commandBuffer();
    {
      auto enc = cmd.startEncoding(device);
      enc.setFramebuffer({{tex,Vec4(0,0,1,1),Tempest::Preserve}});
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
void Draw(const char* outImage) {
  using namespace Tempest;

  try {
    GraphicsApi api{ApiFlags::Validation};
    Device      device(api);

    if(!device.properties().hasAttachFormat(format)) {
      Log::d("Skipping draw testcase: no format support");
      return;
      }

    auto vbo  = device.vbo(vboData,3);
    auto ibo  = device.ibo(iboData,3);

    auto vert = device.shader("shader/simple_test.vert.sprv");
    auto frag = device.shader("shader/simple_test.frag.sprv");
    auto pso  = device.pipeline(Topology::Triangles,RenderState(),vert,frag);

    auto tex  = device.attachment(format,128,128);

    auto cmd  = device.commandBuffer();
    {
      auto enc = cmd.startEncoding(device);
      enc.setFramebuffer({{tex,Vec4(0,0,1,1),Tempest::Preserve}});
      enc.setUniforms(pso);
      enc.draw(vbo,ibo);
    }

    auto sync = device.fence();
    device.submit(cmd,sync);
    sync.wait();

    auto pm = device.readPixels(tex);
    pm.save(outImage);

    const uint32_t ccnt = Pixmap::componentCount(pm.format());
    uint32_t same = 0;
    ImageValidator val(pm);
    for(uint32_t y=0; y<pm.h(); ++y)
      for(uint32_t x=0; x<pm.w(); ++x) {
        auto pix = val.at(x,y);
        auto ref = ImageValidator::Pixel();
        if(x<y) {
          ref.x[0] = 0;
          ref.x[1] = 0;
          ref.x[2] = 1;
          ref.x[3] = 1;
          } else {
          const float u = (float(x)+0.5f)/float(pm.w());
          const float v = (float(y)+0.5f)/float(pm.h());
          ref.x[0] = u;
          ref.x[1] = v;
          ref.x[2] = 0;
          ref.x[3] = 1;
          }

        for(uint32_t c=0; c<ccnt; ++c)
          if(std::fabs(pix.x[c]-ref.x[c])<0.01f)
            same++;
        }
    EXPECT_EQ(same,pm.w()*pm.h()*ccnt);
    }
  catch(std::system_error& e) {
    if(e.code()==Tempest::GraphicsErrc::NoDevice)
      Log::d("Skipping graphics testcase: ", e.what()); else
      throw;
    }
  }

template<class GraphicsApi>
void InstanceIndex(const char* outImage) {
  using namespace Tempest;

  try {
    GraphicsApi api{ApiFlags::Validation};
    Device      device(api);

    auto vbo  = device.vbo(vboData,3);
    auto ibo  = device.ibo(iboData,3);

    auto vert = device.shader("shader/simple_test.vert.sprv");
    auto frag = device.shader("shader/simple_test.frag.sprv");
    auto pso  = device.pipeline(Topology::Triangles,RenderState(),vert,frag);

    auto tex  = device.attachment(Tempest::RGBA8,128,128);

    auto cmd  = device.commandBuffer();
    {
      auto enc = cmd.startEncoding(device);
      enc.setFramebuffer({{tex,Vec4(0,0,1,1),Tempest::Preserve}});
      enc.setUniforms(pso);
      enc.draw(vbo,ibo, 0,ibo.size(), 123,1);
    }

    auto sync = device.fence();
    device.submit(cmd,sync);
    sync.wait();

    auto pm = device.readPixels(tex);
    pm.save(outImage);

    const uint32_t ccnt = Pixmap::componentCount(pm.format());
    uint32_t same = 0;
    ImageValidator val(pm);
    for(uint32_t y=0; y<pm.h(); ++y)
      for(uint32_t x=0; x<pm.w(); ++x) {
        auto pix = val.at(x,y);
        auto ref = ImageValidator::Pixel();
        if(x<y) {
          ref.x[0] = 0;
          ref.x[1] = 0;
          ref.x[2] = 1;
          ref.x[3] = 1;
          } else {
          const float u = (float(x)+0.5f)/float(pm.w());
          const float v = (float(y)+0.5f)/float(pm.h());
          ref.x[0] = u;
          ref.x[1] = v;
          ref.x[2] = 123.f/255.f;
          ref.x[3] = 1;
          }

        for(uint32_t c=0; c<ccnt; ++c)
          if(std::fabs(pix.x[c]-ref.x[c])<0.01f)
            same++;
        }
    EXPECT_EQ(same,pm.w()*pm.h()*ccnt);
    }
  catch(std::system_error& e) {
    if(e.code()==Tempest::GraphicsErrc::NoDevice)
      Log::d("Skipping graphics testcase: ", e.what()); else
      throw;
    }
  }

template<class GraphicsApi>
void Viewport(const char* outImage) {
  using namespace Tempest;

  try {
    GraphicsApi api{ApiFlags::Validation};
    Device      device(api);

    auto vbo  = device.vbo(vboData,3);
    auto ibo  = device.ibo(iboData,3);

    auto vert = device.shader("shader/simple_test.vert.sprv");
    auto frag = device.shader("shader/simple_test.frag.sprv");
    auto pso  = device.pipeline(Topology::Triangles,RenderState(),vert,frag);

    auto tex  = device.attachment(TextureFormat::RGBA8,150,150);

    auto cmd  = device.commandBuffer();
    {
      auto enc = cmd.startEncoding(device);
      enc.setFramebuffer({{tex,Vec4(0,0,1,1),Tempest::Preserve}});
      enc.setUniforms(pso);

      enc.setViewport(-50,25,100,100);
      enc.draw(vbo,ibo);

      enc.setViewport(150,25,100,100);
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
void Uniforms(const char* outImage, bool useUbo) {
  using namespace Tempest;

  struct Ubo {
    Vec4 color[3];
    } data;
  data.color[0] = Vec4(1,0,0,1);
  data.color[1] = Vec4(0,1,0,1);
  data.color[2] = Vec4(0,0,1,1);

  try {
    GraphicsApi api{ApiFlags::Validation};
    Device      device(api);

    auto vbo  = device.vbo(vboData,3);
    auto ibo  = device.ibo(iboData,3);

    auto ubo  = device.ubo(data);
    auto ssbo = device.ssbo(&data,sizeof(data));

    auto vert = device.shader("shader/ubo_input.vert.sprv");
    auto frag = device.shader("shader/simple_test.frag.sprv");
    auto pso  = device.pipeline(Topology::Triangles,RenderState(),vert,frag);

    auto tex  = device.attachment(TextureFormat::RGBA8,128,128);
    auto desc = device.descriptors(pso);
    if(useUbo)
      desc.set(2,ubo); else
      desc.set(2,ssbo);

    auto cmd  = device.commandBuffer();
    {
      auto enc = cmd.startEncoding(device);
      enc.setFramebuffer({{tex,Vec4(0,0,1,1),Tempest::Preserve}});
      enc.setUniforms(pso,desc);
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
void SSBOReadOnly(bool useUbo) {
  using namespace Tempest;
  try {
    GraphicsApi api{ApiFlags::Validation};
    Device      device(api);

    Vec4 data = {1,2,3,4};
    auto ssbo = device.ssbo(&data,  sizeof(data));
    auto ubo  = device.ubo (data);
    auto out  = device.ssbo(nullptr,sizeof(data));

    auto cs   = device.shader("shader/ssbo_read.comp.sprv");
    auto pso  = device.pipeline(cs);

    auto desc = device.descriptors(pso);
    if(useUbo)
      desc.set(0,ubo); else
      desc.set(0,ssbo);
    desc.set(1,out);

    auto cmd  = device.commandBuffer();
    {
      auto enc = cmd.startEncoding(device);
      enc.setUniforms(pso,desc);
      enc.dispatch(1,1,1);
    }

    auto sync = device.fence();
    device.submit(cmd,sync);
    sync.wait();

    Vec4 ret = {};
    device.readBytes(out,&ret,sizeof(ret));
    EXPECT_EQ(ret,data);
    }
  catch(std::system_error& e) {
    if(e.code()==Tempest::GraphicsErrc::NoDevice)
      Log::d("Skipping graphics testcase: ", e.what()); else
      throw;
    }
  }

template<class GraphicsApi>
void UnboundSsbo() {
  using namespace Tempest;
  try {
    GraphicsApi api{ApiFlags::Validation};
    Device      device(api);

    std::vector<Vec4> data;
    data.resize(3000);
    for(size_t i=0; i<data.size(); ++i) {
      data[i].x = float(i)*4.f+0.f;
      data[i].y = float(i)*4.f+1.f;
      data[i].z = float(i)*4.f+2.f;
      data[i].w = float(i)*4.f+3.f;
      }

    auto ssbo = device.ssbo(data.data(), sizeof(data[0])*data.size());
    auto out  = device.ssbo(nullptr,     sizeof(data[0])*data.size());

    auto cs  = device.shader("shader/ssbo_read.comp.sprv");
    auto pso = device.pipeline(cs);

    auto desc = device.descriptors(pso);
    desc.set(0,ssbo);
    desc.set(1,out);

    auto cmd  = device.commandBuffer();
    {
      auto enc = cmd.startEncoding(device);
      enc.setUniforms(pso,desc);
      enc.dispatch(data.size(),1,1);
    }

    auto sync = device.fence();
    device.submit(cmd,sync);
    sync.wait();

    std::vector<Vec4> ret(data.size());
    device.readBytes(out,ret.data(),sizeof(data[0])*data.size());
    EXPECT_EQ(ret,data);
    }
  catch(std::system_error& e) {
    if(e.code()==Tempest::GraphicsErrc::NoDevice)
      Log::d("Skipping graphics testcase: ", e.what()); else
      throw;
    }
  }

template<class GraphicsApi>
void SbboOverlap() {
  using namespace Tempest;

  struct Buf {
    uint32_t src = 1;
    uint32_t dst = 2;
    uint32_t padd[2] = {};
    Vec4     a = {1,2,3,4};
    Vec4     b = {};
    };

  try {
    GraphicsApi api{ApiFlags::Validation};
    Device      device(api);

    Buf  buf;
    auto ssbo   = device.ssbo(&buf,sizeof(buf));

    auto cs     = device.shader("shader/overlap_test.comp.sprv");
    auto pso    = device.pipeline(cs);

    auto ubo    = device.descriptors(pso.layout());
    ubo.set(0,ssbo);

    auto cmd = device.commandBuffer();
    {
      auto enc = cmd.startEncoding(device);
      enc.setUniforms(pso,ubo);
      enc.dispatch(1);
    }

    auto sync = device.fence();
    device.submit(cmd,sync);
    sync.wait();

    Buf output;
    device.readBytes(ssbo,&output,sizeof(output));

    EXPECT_EQ(buf.a,output.b);
    }
  catch(std::system_error& e) {
    if(e.code()==Tempest::GraphicsErrc::NoDevice)
      Log::d("Skipping graphics testcase: ", e.what()); else
      throw;
    }
  }

template<class GraphicsApi>
void Compute() {
  using namespace Tempest;

  try {
    GraphicsApi api{ApiFlags::Validation};
    Device      device(api);

    Vec4 inputCpu[3] = {Vec4(0,1,2,3),Vec4(4,5,6,7),Vec4(8,9,10,11)};

    auto input  = device.ssbo(inputCpu,sizeof(inputCpu));
    auto output = device.ssbo(nullptr, sizeof(inputCpu));

    auto cs     = device.shader("shader/simple_test.comp.sprv");
    auto pso    = device.pipeline(cs);

    auto ubo    = device.descriptors(pso.layout());
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
    device.readBytes(output,outputCpu,sizeof(outputCpu));

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
void ComputeImage(const char* outImage) {
  using namespace Tempest;

  try {
    GraphicsApi api{ApiFlags::Validation};
    Device      device(api);

    auto img = device.image2d(TextureFormat::RGBA8,128,128,false);
    auto pso = device.pipeline(device.shader("shader/image_store_test.comp.sprv"));

    auto ubo = device.descriptors(pso.layout());
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
void MipMaps(const char* outImage) {
  using namespace Tempest;

  try {
    GraphicsApi api{ApiFlags::Validation};
    Device      device(api);

    auto vbo  = device.vbo(vboData,3);
    auto ibo  = device.ibo(iboData,3);

    auto vert = device.shader("shader/simple_test.vert.sprv");
    auto frag = device.shader("shader/simple_test.frag.sprv");
    auto pso  = device.pipeline(Topology::Triangles,RenderState(),vert,frag);

    auto tex  = device.attachment(format,128,128,true);
    auto sync = device.fence();

    auto cmd = device.commandBuffer();
    {
      auto enc = cmd.startEncoding(device);
      enc.setFramebuffer({{tex,Vec4(0,0,1,1),Tempest::Preserve}});
      enc.setUniforms(pso);
      enc.draw(vbo,ibo);
      enc.setFramebuffer({});
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

template<class GraphicsApi>
void S3TC(const char* /*outImage*/) {
  using namespace Tempest;
  try {
    GraphicsApi api{ApiFlags::Validation};
    Device      device(api);

    auto src = Pixmap("data/img/tst-dxt5.dds");
    auto tex = device.texture(src);
    EXPECT_EQ(tex.format(),TextureFormat::DXT5);

    auto dst = device.readPixels(tex);
    EXPECT_EQ(dst.format(),Pixmap::Format::DXT5);

    // EXPECT_EQ(dst.dataSize(),src.dataSize());
    EXPECT_TRUE(std::memcmp(dst.data(),src.data(),dst.dataSize())==0);
    }
  catch(std::system_error& e) {
    if(e.code()==Tempest::GraphicsErrc::NoDevice)
      Log::d("Skipping directx testcase: ", e.what()); else
      throw;
    }
  }

template<class GraphicsApi>
void SsboWrite() {
  using namespace Tempest;

  try {
    GraphicsApi api{ApiFlags::Validation};
    Device      device(api);

    if(!device.properties().storeAndAtomicVs) {
      Log::d("Skipping graphics testcase: storeAndAtomicVs is not supported");
      return;
      }

    // setup draw-pipeline
    auto vbo    = device.vbo(vboData,3);
    auto ibo    = device.ibo(iboData,3);

    auto vert   = device.shader("shader/ssbo_write.vert.sprv");
    auto frag   = device.shader("shader/simple_test.frag.sprv");
    auto pso    = device.pipeline(Topology::Triangles,RenderState(),vert,frag);

    auto tex    = device.attachment(TextureFormat::RGBA8,32,32);
    auto vsOut  = device.ssbo(nullptr, sizeof(Vec4)*3);
    auto ubo    = device.descriptors(pso.layout());
    ubo.set(0,vsOut);

    // setup compute pipeline
    auto cs     = device.shader("shader/ssbo_write_verify.comp.sprv");
    auto pso2   = device.pipeline(cs);
    auto csOut  = device.ssbo(nullptr, sizeof(Vec4)*3);

    auto ubo2   = device.descriptors(pso2.layout());
    ubo2.set(0,vsOut);
    ubo2.set(1,csOut);

    auto cmd = device.commandBuffer();
    {
      auto enc = cmd.startEncoding(device);
      enc.setFramebuffer({{tex,Vec4(0,0,1,1),Tempest::Preserve}});
      enc.setUniforms(pso,ubo);
      enc.draw(vbo,ibo);

      enc.setFramebuffer({});
      enc.setUniforms(pso2,ubo2);
      enc.dispatch(3,1,1);
    }

    auto sync = device.fence();
    device.submit(cmd,sync);
    sync.wait();

    Vec4 outputCpu[3] = {};
    device.readBytes(csOut,outputCpu,sizeof(outputCpu));

    for(size_t i=0; i<3; ++i) {
      EXPECT_EQ(outputCpu[i].x,vboData[i].x);
      EXPECT_EQ(outputCpu[i].y,vboData[i].y);
      EXPECT_EQ(size_t(outputCpu[i].z),i);
      }
    }
  catch(std::system_error& e) {
    if(e.code()==Tempest::GraphicsErrc::NoDevice)
      Log::d("Skipping graphics testcase: ", e.what()); else
      throw;
    }
  }

template<class GraphicsApi>
void ComponentSwizzle() {
  using namespace Tempest;
  try {
    GraphicsApi api{ApiFlags::Validation};
    Device      device(api);

    static const Vertex vboData[6] = {
      {-1,-1},{1,-1},{1,1},
      {-1,-1},{1,1},{-1,1},
    };
    auto vbo = device.vbo(vboData,6);

    Pixmap pm(1,1,Pixmap::Format::RGBA);
    {
    auto p = reinterpret_cast<uint8_t*>(pm.data());
    p[0] = 255;
    p[1] = 128;
    p[2] = 64;
    p[3] = 32;
    }

    auto vert   = device.shader("shader/texture.vert.sprv");
    auto frag   = device.shader("shader/texture.frag.sprv");
    auto pso    = device.pipeline(Topology::Triangles,RenderState(),vert,frag);
    auto tex    = device.texture(pm,false);

    DescriptorSet ubo[4];
    for(int i=0; i<4; ++i) {
      ubo[i] = device.descriptors(pso.layout());

      Sampler smp = Sampler::nearest();
      smp.mapping.r = Tempest::ComponentSwizzle(int(ComponentSwizzle::R) + i);
      smp.mapping.g = smp.mapping.r;
      smp.mapping.b = smp.mapping.r;
      smp.mapping.a = smp.mapping.r;
      ubo[i].set(0,tex,smp);
      }

    auto fbo    = device.attachment(TextureFormat::RGBA8,2,2,false);
    auto cmd    = device.commandBuffer();
    {
      auto enc = cmd.startEncoding(device);
      enc.setFramebuffer({{fbo,Vec4(0,0,0,0),Tempest::Preserve}});

      enc.setUniforms(pso,ubo[0]);
      enc.setScissor(0,0,1,1);
      enc.draw(vbo);

      enc.setUniforms(pso,ubo[1]);
      enc.setScissor(1,0,1,1);
      enc.draw(vbo);

      enc.setUniforms(pso,ubo[2]);
      enc.setScissor(0,1,1,1);
      enc.draw(vbo);

      enc.setUniforms(pso,ubo[3]);
      enc.setScissor(1,1,1,1);
      enc.draw(vbo);
    }

    auto sync = device.fence();
    device.submit(cmd,sync);
    sync.wait();

    {
    auto ref = reinterpret_cast<const uint8_t*>(pm.data());
    auto ret = device.readPixels(fbo);

    auto    p   = reinterpret_cast<uint8_t*>(ret.data());
    for(int i=0; i<4; ++i) {
      EXPECT_EQ(p[i*4+0], ref[i]);
      EXPECT_EQ(p[i*4+1], ref[i]);
      EXPECT_EQ(p[i*4+2], ref[i]);
      EXPECT_EQ(p[i*4+3], ref[i]);
      }
    }
    }
  catch(std::system_error& e) {
    if(e.code()==Tempest::GraphicsErrc::NoDevice)
      Log::d("Skipping graphics testcase: ", e.what()); else
      throw;
    }
  }

template<class GraphicsApi>
void PushRemapping() {
  using namespace Tempest;

  struct Push {
    Vec4 vPush = Vec4(0,1,2,3);
    float val2 = 0;
    };

  try {
    GraphicsApi api{ApiFlags::Validation};
    Device      device(api);

    Push inputCpu;

    auto input  = device.ubo (&inputCpu,sizeof(Tempest::Vec4));
    auto output = device.ssbo(nullptr,  sizeof(Tempest::Vec4));

    auto cs     = device.shader("shader/push_constant.comp.sprv");
    auto pso    = device.pipeline(cs);

    auto ubo = device.descriptors(pso.layout());
    ubo.set(0,input);
    ubo.set(1,output);

    auto cmd = device.commandBuffer();
    {
      auto enc = cmd.startEncoding(device);
      enc.setUniforms(pso,ubo,&inputCpu,sizeof(inputCpu));
      enc.dispatch(1,1,1);
    }

    auto sync = device.fence();
    device.submit(cmd,sync);
    sync.wait();

    Vec4 outputCpu = {};
    device.readBytes(output,&outputCpu,sizeof(outputCpu));

    EXPECT_EQ(outputCpu,Vec4(1,1,1,1));
    }
  catch(std::system_error& e) {
    if(e.code()==Tempest::GraphicsErrc::NoDevice)
      Log::d("Skipping graphics testcase: ", e.what()); else
      throw;
    }
  }

template<class GraphicsApi>
void PushRemappingGr(const char* outImage) {
  using namespace Tempest;

  try {
    GraphicsApi api{ApiFlags::Validation};
    Device      device(api);

    auto push = Vec4(1,1,1,1);
    auto vbo  = device.vbo(vboData,3);
    auto ibo  = device.ibo(iboData,3);
    auto ssbo = device.ssbo<float>({0.5,0.5,0.5,0.5});

    auto vert = device.shader("shader/push_test.vert.sprv");
    auto frag = device.shader("shader/push_test.frag.sprv");
    auto pso  = device.pipeline(Topology::Triangles,RenderState(),vert,frag);
    auto desc = device.descriptors(pso);
    desc.set(0,ssbo);

    auto tex  = device.attachment(Tempest::RGBA8,128,128);

    auto cmd  = device.commandBuffer();
    {
      auto enc = cmd.startEncoding(device);
      enc.setFramebuffer({{tex,Vec4(0,0,1,1),Tempest::Preserve}});
      enc.setUniforms(pso,desc,&push,sizeof(push));
      enc.draw(vbo,ibo, 0,ibo.size());
    }

    auto sync = device.fence();
    device.submit(cmd,sync);
    sync.wait();

    auto pm = device.readPixels(tex);
    pm.save(outImage);

    const uint32_t ccnt = Pixmap::componentCount(pm.format());
    uint32_t same = 0;
    ImageValidator val(pm);
    for(uint32_t y=0; y<pm.h(); ++y)
      for(uint32_t x=0; x<pm.w(); ++x) {
        auto pix = val.at(x,y);
        auto ref = ImageValidator::Pixel();
        if(x<y) {
          ref.x[0] = 0;
          ref.x[1] = 0;
          ref.x[2] = 1;
          ref.x[3] = 1;
          } else {
          ref.x[0] = 0.5;
          ref.x[1] = 0.5;
          ref.x[2] = 1;
          ref.x[3] = 1;
          }

        for(uint32_t c=0; c<ccnt; ++c)
          if(std::fabs(pix.x[c]-ref.x[c])<0.01f)
            same++;
        }
    EXPECT_EQ(same,pm.w()*pm.h()*ccnt);
    }
  catch(std::system_error& e) {
    if(e.code()==Tempest::GraphicsErrc::NoDevice)
      Log::d("Skipping graphics testcase: ", e.what()); else
      throw;
    }
  }

template<class GraphicsApi>
void ArrayedTextures(const char* outImg) {
  using namespace Tempest;
  try {
    const char* devName = nullptr;

    GraphicsApi api{ApiFlags::Validation};
    auto dev = api.devices();
    for(auto& i:dev)
      if(i.bindless.nonUniformIndexing)
        devName = i.name;
    if(devName==nullptr)
      return;
    Device device(api,devName);

    auto cs  = device.shader("shader/array_texture.comp.sprv");
    auto pso = device.pipeline(cs);
    auto ret = device.image2d(TextureFormat::RGBA8,128,128,false);

    std::vector<Tempest::Texture2d> tex(2);
    tex[0] = device.texture("data/img/texture.png");
    tex[1] = device.texture("data/img/tst-dxt5.dds");

    std::vector<const Tempest::Texture2d*> ptex(tex.size());
    for(size_t i=0; i<tex.size(); ++i)
      ptex[i] = &tex[i];

    auto desc = device.descriptors(pso);
    desc.set(0,ret);
    desc.set(1,ptex);

    auto cmd = device.commandBuffer();
    {
      auto enc = cmd.startEncoding(device);
      enc.setUniforms(pso,desc);
      enc.dispatch(ret.w(),ret.h(),1);
    }

    auto sync = device.fence();
    device.submit(cmd,sync);
    sync.wait();

    auto pm = device.readPixels(ret);
    pm.save(outImg);
    }
  catch(std::system_error& e) {
    if(e.code()==Tempest::GraphicsErrc::NoDevice)
      Log::d("Skipping graphics testcase: ", e.what()); else
      throw;
    }
  }

template<class GraphicsApi>
void ArrayedImages(const char* outImg) {
  using namespace Tempest;
  try {
    const char* devName = nullptr;

    GraphicsApi api{ApiFlags::Validation};
    auto dev = api.devices();
    for(auto& i:dev)
      if(i.bindless.nonUniformIndexing)
        devName = i.name;
    if(devName==nullptr)
      return;
    Device device(api,devName);

    auto cs  = device.shader("shader/array_image.comp.sprv");
    auto pso = device.pipeline(cs);
    auto ret = device.image2d(TextureFormat::RGBA8,128,128,false);

    std::vector<Tempest::Texture2d> tex(2);
    tex[0] = device.texture("data/img/texture.png");
    tex[1] = device.texture("data/img/tst-dxt5.dds");

    std::vector<const Tempest::Texture2d*> ptex(tex.size());
    for(size_t i=0; i<tex.size(); ++i)
      ptex[i] = &tex[i];

    auto desc = device.descriptors(pso);
    desc.set(0,ret);
    desc.set(1,ptex);
    desc.set(2,Sampler::bilinear());

    auto cmd = device.commandBuffer();
    {
      auto enc = cmd.startEncoding(device);
      enc.setUniforms(pso,desc);
      enc.dispatch(ret.w(),ret.h(),1);
    }

    auto sync = device.fence();
    device.submit(cmd,sync);
    sync.wait();

    auto pm = device.readPixels(ret);
    pm.save(outImg);
    }
  catch(std::system_error& e) {
    if(e.code()==Tempest::GraphicsErrc::NoDevice)
      Log::d("Skipping graphics testcase: ", e.what()); else
      throw;
    }
  }

template<class GraphicsApi>
void ArrayedSsbo(const char* outImg) {
  using namespace Tempest;
  try {
    const char* devName = nullptr;

    GraphicsApi api{ApiFlags::Validation};
    auto dev = api.devices();
    for(auto& i:dev)
      if(i.bindless.nonUniformIndexing)
        devName = i.name;
    if(devName==nullptr)
      return;
    Device device(api,devName);

    auto cs  = device.shader("shader/array_ssbo.comp.sprv");
    auto pso = device.pipeline(cs);
    auto ret = device.image2d(TextureFormat::RGBA8,128,128,false);

    std::vector<Tempest::StorageBuffer> buf(3);
    buf[0] = device.ssbo<Vec4>({Vec4(1,0,0,1)});
    buf[1] = device.ssbo<Vec4>({Vec4(0,1,0,1)});
    buf[2] = device.ssbo<Vec4>({Vec4(0,0,1,1)});

    std::vector<const Tempest::StorageBuffer*> pbuf(buf.size());
    for(size_t i=0; i<buf.size(); ++i)
      pbuf[i] = &buf[i];

    auto desc = device.descriptors(pso);
    desc.set(0,ret);
    desc.set(1,pbuf);

    auto cmd = device.commandBuffer();
    {
      auto enc = cmd.startEncoding(device);
      enc.setUniforms(pso,desc);
      enc.dispatch(ret.w(),ret.h(),1);
    }

    auto sync = device.fence();
    device.submit(cmd,sync);
    sync.wait();

    auto pm = device.readPixels(ret);
    pm.save(outImg);
    }
  catch(std::system_error& e) {
    if(e.code()==Tempest::GraphicsErrc::NoDevice)
      Log::d("Skipping graphics testcase: ", e.what()); else
      throw;
    }
  }

template<class GraphicsApi>
void Bindless(const char* outImg) {
  using namespace Tempest;
  try {
    const char* devName = nullptr;

    GraphicsApi api{ApiFlags::Validation};
    auto dev = api.devices();
    for(auto& i:dev)
      if(i.bindless.nonUniformIndexing)
        devName = i.name;
    if(devName==nullptr)
      return;

    Device device(api,devName);

    auto cs  = device.shader("shader/bindless.comp.sprv");
    auto pso = device.pipeline(cs);
    auto ret = device.image2d(TextureFormat::RGBA8,128,128,false);

    std::vector<Tempest::Texture2d> tex(2);
    tex[0] = device.texture("data/img/texture.png");
    tex[1] = device.texture("data/img/tst-dxt5.dds");

    std::vector<const Tempest::Texture2d*> ptex(tex.size());
    for(size_t i=0; i<tex.size(); ++i)
      ptex[i] = &tex[i];

    auto desc = device.descriptors(pso);
    desc.set(0,ptex);
    desc.set(1,ret);

    auto cmd = device.commandBuffer();
    {
      auto enc = cmd.startEncoding(device);
      enc.setUniforms(pso,desc);
      enc.dispatch(ret.w(),ret.h(),1);
    }

    auto sync = device.fence();
    device.submit(cmd,sync);
    sync.wait();

    auto pm = device.readPixels(ret);
    pm.save(outImg);
    }
  catch(std::system_error& e) {
    if(e.code()==Tempest::GraphicsErrc::NoDevice)
      Log::d("Skipping graphics testcase: ", e.what()); else
      throw;
    }
  }

template<class GraphicsApi>
void Bindless2(const char* outImg) {
  using namespace Tempest;
  try {
    const char* devName = nullptr;

    GraphicsApi api{ApiFlags::Validation};
    auto dev = api.devices();
    for(auto& i:dev)
      if(i.bindless.nonUniformIndexing)
        devName = i.name;
    if(devName==nullptr)
      return;

    Device device(api,devName);

    auto cs  = device.shader("shader/bindless2.comp.sprv");
    auto pso = device.pipeline(cs);
    auto ret = device.image2d(TextureFormat::RGBA8,128,128,false);

    std::vector<Tempest::Texture2d> tex(2);
    tex[0] = device.texture("data/img/texture.png");
    tex[1] = device.texture("data/img/tst-dxt5.dds");

    std::vector<Tempest::StorageBuffer> buf(3);
    buf[0] = device.ssbo<Vec4>({Vec4(1,0,0,1)});
    buf[1] = device.ssbo<Vec4>({Vec4(0,1,0,1)});
    buf[2] = device.ssbo<Vec4>({Vec4(0,0,1,1)});

    std::vector<const Tempest::Texture2d*> ptex(tex.size());
    for(size_t i=0; i<tex.size(); ++i)
      ptex[i] = &tex[i];

    std::vector<const Tempest::StorageBuffer*> pbuf(buf.size());
    for(size_t i=0; i<buf.size(); ++i)
      pbuf[i] = &buf[i];

    auto desc = device.descriptors(pso);
    desc.set(0,ptex);
    desc.set(1,pbuf);
    desc.set(2,ret);

    auto cmd = device.commandBuffer();
    {
      auto enc = cmd.startEncoding(device);
      enc.setUniforms(pso,desc);
      enc.dispatch(ret.w(),ret.h(),1);
    }

    auto sync = device.fence();
    device.submit(cmd,sync);
    sync.wait();

    auto pm = device.readPixels(ret);
    pm.save(outImg);
    }
  catch(std::system_error& e) {
    if(e.code()==Tempest::GraphicsErrc::NoDevice)
      Log::d("Skipping graphics testcase: ", e.what()); else
      throw;
    }
  }

template<class GraphicsApi>
void Blas() {
  using namespace Tempest;

  try {
    const char* rtDev = nullptr;

    GraphicsApi api{ApiFlags::Validation};
    auto dev = api.devices();
    for(auto& i:dev)
      if(i.raytracing.rayQuery)
        rtDev = i.name;
    if(rtDev==nullptr)
      return;

    Device device(api,rtDev);
    auto vbo  = device.vbo(vboData3,3);
    auto ibo  = device.ibo(iboData,3);
    auto blas = device.blas(vbo,ibo);
    auto tlas = device.tlas({{Matrix4x4::mkIdentity(), 0, &blas}});
    }
  catch(std::system_error& e) {
    if(e.code()==Tempest::GraphicsErrc::NoDevice)
      Log::d("Skipping graphics testcase: ", e.what()); else
      throw;
    }
  }

template<class GraphicsApi>
void RayQuery(const char* outImg) {
  using namespace Tempest;

  try {
    const char* rtDev = nullptr;

    GraphicsApi api{ApiFlags::Validation};
    auto dev = api.devices();
    for(auto& i:dev)
      if(i.raytracing.rayQuery)
        rtDev = i.name;
    if(rtDev==nullptr)
      return;

    Device device(api,rtDev);
    auto vbo  = device.vbo(vboData3,3);
    auto ibo  = device.ibo(iboData,3);
    auto blas = device.blas(vbo,ibo);

    auto m = Matrix4x4::mkIdentity();
    m.translate(-1,1,0);
    auto tlas = device.tlas({{m,0,&blas}});

    auto fsq  = device.vbo<Vertex>({{-1,-1},{ 1,-1},{ 1, 1}, {-1,-1},{ 1, 1},{-1, 1}});
    auto vert = device.shader("shader/simple_test.vert.sprv");
    auto frag = device.shader("shader/ray_test.frag.sprv");
    auto pso  = device.pipeline(Topology::Triangles,RenderState(),vert,frag);

    auto ubo  = device.descriptors(pso);
    ubo.set(0, tlas);

    auto tex = device.attachment(TextureFormat::RGBA8,128,128);
    auto cmd = device.commandBuffer();
    {
      auto enc = cmd.startEncoding(device);
      enc.setFramebuffer({{tex,Vec4(0,0,1,1),Tempest::Preserve}});
      enc.setUniforms(pso,ubo);
      enc.draw(fsq);
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

template<class GraphicsApi>
void RayQueryFace(const char* outImg) {
  using namespace Tempest;

  try {
    const char* rtDev = nullptr;

    GraphicsApi api{ApiFlags::Validation};
    auto dev = api.devices();
    for(auto& i:dev)
      if(i.raytracing.rayQuery)
        rtDev = i.name;
    if(rtDev==nullptr)
      return;

    Device device(api,rtDev);

    const Tempest::Vec3 vboData[4] = {{-1,-1,0},{ 1,-1,0},{1,1,0},{-1, 1,0}};
    const uint16_t      iboData[6] = {0,1,2, 0,3,2};
    auto vbo  = device.vbo(vboData,4);
    auto ibo  = device.ibo(iboData,6);
    auto blas = device.blas(vbo,ibo);

    auto m = Matrix4x4::mkIdentity();
    m.translate(-1,1,0);
    auto tlas = device.tlas({{m,0,&blas}});

    auto fsq  = device.vbo<Vertex>({{-1,-1},{ 1,-1},{ 1, 1}, {-1,-1},{ 1, 1},{-1, 1}});
    auto vert = device.shader("shader/simple_test.vert.sprv");
    auto frag = device.shader("shader/ray_test_face.frag.sprv");
    auto pso  = device.pipeline(Topology::Triangles,RenderState(),vert,frag);

    auto ubo  = device.descriptors(pso);
    ubo.set(0, tlas);

    auto tex = device.attachment(TextureFormat::RGBA8,128,128);
    auto cmd = device.commandBuffer();
    {
      auto enc = cmd.startEncoding(device);
      enc.setFramebuffer({{tex,Vec4(0,0,1,1),Tempest::Preserve}});
      enc.setUniforms(pso,ubo);
      enc.draw(fsq);
    }
    auto sync = device.fence();
    device.submit(cmd,sync);
    sync.wait();

    auto pm = device.readPixels(tex);
    pm.save(outImg);

    const uint32_t* px = reinterpret_cast<const uint32_t*>(pm.data());
    EXPECT_EQ(px[ 0 + 127*pm.w()], 0xFF00FF00);
    EXPECT_EQ(px[63 +  64*pm.w()], 0xFF0000FF);
    }
  catch(std::system_error& e) {
    if(e.code()==Tempest::GraphicsErrc::NoDevice)
      Log::d("Skipping graphics testcase: ", e.what()); else
      throw;
    }
  }

template<class GraphicsApi>
void MeshShader(const char* outImg) {
  using namespace Tempest;

  try {
    const char* msDev = nullptr;

    GraphicsApi api{ApiFlags::Validation};
    auto dev = api.devices();
    for(auto& i:dev)
      if(i.meshlets.meshShader)
        msDev = i.name;
    if(msDev==nullptr)
      return;

    Device device(api,msDev);
    auto vbo  = device.vbo(vboData,3);

    auto task = Tempest::Shader(); //device.shader("shader/simple_test.spv14.task.sprv");
    auto mesh = device.shader("shader/simple_test.spv14.mesh.sprv");
    auto frag = device.shader("shader/simple_test.frag.sprv");
    auto pso  = device.pipeline(RenderState(),task,mesh,frag);

    auto ubo  = device.descriptors(pso);
    ubo.set(0, vbo);

    auto tex = device.attachment(TextureFormat::RGBA8,128,128);
    auto cmd = device.commandBuffer();
    {
      auto enc = cmd.startEncoding(device);
      enc.setFramebuffer({{tex,Vec4(0,0,1,1),Tempest::Preserve}});
      enc.setUniforms(pso,ubo);
      enc.dispatchMesh(1);
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

template<class GraphicsApi>
void MeshShaderEmulated(const char* outImg) {
  using namespace Tempest;

  try {
    const char* msDev = nullptr;

    GraphicsApi api{ApiFlags::Validation};
    auto dev = api.devices();
    for(auto& i:dev)
      if(i.meshlets.meshShaderEmulated)
        msDev = i.name;
    if(msDev==nullptr)
      return;

    Device device(api,msDev);
    auto vbo  = device.vbo(vboData,3);

    auto mesh = device.shader("shader/simple_test.mesh.sprv");
    auto frag = device.shader("shader/simple_test.frag.sprv");
    auto pso  = device.pipeline(RenderState(),Tempest::Shader(),mesh,frag);

    auto ubo  = device.descriptors(pso);
    ubo.set(0, vbo);

    auto tex = device.attachment(TextureFormat::RGBA8,128,128);
    auto cmd = device.commandBuffer();
    {
      auto enc = cmd.startEncoding(device);
      enc.setFramebuffer({{tex,Vec4(0,0,1,1),Tempest::Preserve}});
      enc.setUniforms(pso,ubo);
      enc.dispatchMesh(1);
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

template<class GraphicsApi>
void MeshComputePrototype(const char* outImg) {
  using namespace Tempest;

  struct VkDrawIndexedIndirectCommand {
    uint32_t    indexCount    = 0;
    uint32_t    instanceCount = 0;
    uint32_t    firstIndex    = 0;
    int32_t     vertexOffset  = 0;
    uint32_t    firstInstance = 0;

    uint32_t    vboOffset     = 0;
    };

  enum : uint32_t {
    B_Indirect = 10,
    B_Meshlet  = 11,
    B_Var      = 12,
  };

  try {
    const char* msDev = nullptr;

    GraphicsApi api{ApiFlags::Validation};

    Device device(api,msDev);
    auto vbo  = device.vbo(vboData,3);

    VkDrawIndexedIndirectCommand ix[2] = {};
    auto indirect = device.ssbo(ix,sizeof(ix));

    auto var  = device.ssbo(nullptr, 4*16*1024);      // big buffer for meshlet data
    auto flat = device.ssbo(nullptr, var.byteSize()); // double buffer
    auto mesh = device.ssbo(nullptr, 4*256);          // buffer meshlet descriptors

    const uint32_t msz[4] = {0,0,1,1};
    mesh.update(&msz,0,sizeof(msz));

    auto shaderMs = device.shader("shader/simple_test.mesh.comp.sprv");
    auto psoMs    = device.pipeline(shaderMs);

    auto ubo0  = device.descriptors(psoMs);
    ubo0.set(0,  vbo);
    ubo0.set(B_Indirect, indirect);
    ubo0.set(B_Meshlet,  mesh);
    ubo0.set(B_Var,      var);

    auto ubo1  = device.descriptors(psoMs);
    ubo1.set(0,  vbo);
    ubo1.set(B_Indirect, indirect);
    ubo1.set(B_Meshlet,  mesh);
    ubo1.set(B_Var,      var);

    auto shSum  = device.shader("shader/mesh_prefix_sum.comp.sprv");
    auto psoSum = device.pipeline(shSum);
    auto uboSum = device.descriptors(psoSum);
    uboSum.set(0, indirect);
    uboSum.set(1, mesh);
    uboSum.set(2, var);

    auto shCompactage  = device.shader("shader/mesh_compactage.comp.sprv");
    auto psoCompactage = device.pipeline(shCompactage);
    auto uboCompactage = device.descriptors(psoCompactage);
    uboCompactage.set(1,          flat);
    uboCompactage.set(B_Indirect, indirect);
    uboCompactage.set(B_Meshlet,  mesh);
    uboCompactage.set(B_Var,      var);

    auto cmd = device.commandBuffer();
    {
      auto enc = cmd.startEncoding(device);
      uint32_t id = 0;
      enc.setUniforms(psoMs,ubo0,&id,sizeof(id));
      enc.dispatch(3, 1,1);
      id = 1;
      enc.setUniforms(psoMs,ubo1,&id,sizeof(id));
      enc.dispatch(2, 1,1);
      // ^ 3+2 meshlets

      // prefix summ pass
      enc.setUniforms(psoSum,uboSum);
      enc.dispatch(1,1,1);

      // should be dispatch-indirect
      enc.setUniforms(psoCompactage,uboCompactage);
      enc.dispatch(5,1,1);
      // should be draw-indirect at the end
    }
    auto sync = device.fence();
    device.submit(cmd,sync);
    sync.wait();

    std::vector<uint32_t> meshCpu(mesh.byteSize()/4);
    std::vector<uint32_t> varCpu (var.byteSize()/4);
    device.readBytes(mesh,    meshCpu.data(), mesh.byteSize());
    device.readBytes(var,     varCpu.data(),  var.byteSize() );
    device.readBytes(indirect,ix,indirect.byteSize());

    std::vector<uint32_t> flatCpu(flat.byteSize()/4);
    device.readBytes(flat,flatCpu.data(),flat.byteSize());

    EXPECT_EQ(meshCpu[1],5);
    for(uint32_t i=0; i<meshCpu[1]; ++i) {
      uint32_t self = meshCpu[4+i*3+0];
      uint32_t heap = meshCpu[4+i*3+1];
      uint32_t desc = meshCpu[4+i*3+2];

      uint32_t indSize   = (desc       ) & 0x3FF;
      uint32_t maxVertex = (desc >> 10 ) & 0xFF;
      uint32_t varSize   = (desc >> 18u);

      Log::i("{self=",self,", heap=",heap,", gl_PrimitiveCountNV=",indSize,", varSize=",varSize,", maxVertex=",maxVertex,"}");
      }

    for(uint32_t i=0; i<2; ++i) {
      Log::i("{indexCount=",ix[i].indexCount,", instanceCount=",ix[i].instanceCount,
             ", firstIndex=",ix[i].firstIndex,", vertexOffset=",ix[i].vertexOffset,
             ", firstInstance=",ix[i].firstInstance, ", v = ",ix[i].vboOffset,"}");
      }

    for(uint32_t i=0; i<2; ++i) {
      std::stringstream ibo,vbo;
      for(uint32_t r=0; r<ix[i].indexCount; ++r) {
        uint32_t idx = flatCpu[ix[i].firstIndex+r];
        ibo << idx << ", ";

        auto *v = reinterpret_cast<const float*>(&flatCpu[idx]);
        vbo << "{" << v[0] <<", " << v[1] << ", " << v[2] << "}, ";
        }
      Log::d(ibo.str());
      Log::d(vbo.str());
      }

    EXPECT_EQ(ix[0].indexCount,9);
    EXPECT_EQ(ix[1].indexCount,6);
    }
  catch(std::system_error& e) {
    if(e.code()==Tempest::GraphicsErrc::NoDevice)
      Log::d("Skipping graphics testcase: ", e.what()); else
      throw;
    }
  }
}
