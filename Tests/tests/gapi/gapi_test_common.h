#pragma once

#include <Tempest/Device>
#include <Tempest/Except>
#include <Tempest/Fence>
#include <Tempest/Pixmap>
#include <Tempest/Log>
#include <Tempest/Vec>

#include <gtest/gtest.h>
#include <gmock/gmock-matchers.h>

#include "utils/imagevalidator.h"

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
    EXPECT_EQ(ssboD.size(),sizeof(vboData));
    EXPECT_EQ(ssboR.size(),sizeof(vboData));
    EXPECT_EQ(ssboU.size(),sizeof(vboData));
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

    device.readBytes(ssbo,readback,ssbo.size());
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

    for(size_t i=0; i<eltCount; ++i) {
      T val = data[i];
      EXPECT_EQ(val,T(i*i));
      }
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
    auto pso  = device.pipeline<Vertex>(Topology::Triangles,RenderState(),vert,frag);
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
    auto pso  = device.pipeline<Vertex>(Topology::Triangles,RenderState(),vert,tesc,tese,frag);
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
    auto pso  = device.pipeline<Vertex>(Topology::Triangles,RenderState(),vert,frag);

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
void Viewport(const char* outImage) {
  using namespace Tempest;

  try {
    GraphicsApi api{ApiFlags::Validation};
    Device      device(api);

    auto vbo  = device.vbo(vboData,3);
    auto ibo  = device.ibo(iboData,3);

    auto vert = device.shader("shader/simple_test.vert.sprv");
    auto frag = device.shader("shader/simple_test.frag.sprv");
    auto pso  = device.pipeline<Vertex>(Topology::Triangles,RenderState(),vert,frag);

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

template<class GraphicsApi, Tempest::TextureFormat format>
void uniforms(const char* outImage) {
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

    auto vert = device.shader("shader/ubo_input.vert.sprv");
    auto frag = device.shader("shader/simple_test.frag.sprv");
    auto pso  = device.pipeline<Vertex>(Topology::Triangles,RenderState(),vert,frag);

    auto tex  = device.attachment(format,128,128);
    auto ubo  = device.ubo(data);
    auto desc = device.descriptors(pso);
    desc.set(2,ubo);

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
    auto pso  = device.pipeline<Vertex>(Topology::Triangles,RenderState(),vert,frag);

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
    auto pso    = device.pipeline<Vertex>(Topology::Triangles,RenderState(),vert,frag);

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
}
