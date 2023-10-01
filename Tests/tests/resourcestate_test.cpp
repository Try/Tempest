#include "../gapi/resourcestate.h"

#include <Tempest/Log>

#include <gtest/gtest.h>
#include <gmock/gmock-matchers.h>

using namespace testing;

using namespace Tempest;
using namespace Tempest::Detail;

static const char* toString(ResourceAccess rs) {
  switch (rs) {
    case ResourceAccess::None:          return "None";
    case ResourceAccess::TransferSrc:   return "TransferSrc";
    case ResourceAccess::TransferDst:   return "TransferDst";
    case ResourceAccess::Present:       return "Present";
    case ResourceAccess::Sampler:       return "Sampler";
    case ResourceAccess::ColorAttach:   return "ColorAttach";
    case ResourceAccess::DepthAttach:   return "DepthAttach";
    case ResourceAccess::DepthReadOnly: return "DepthReadOnly";
    case ResourceAccess::Index:         return "Index";
    case ResourceAccess::Vertex:        return "Vertex";
    case ResourceAccess::Uniform:       return "Uniform";
    case ResourceAccess::UavReadComp:
    case ResourceAccess::UavWriteComp:
    case ResourceAccess::UavReadGr:
    case ResourceAccess::UavWriteGr:
    case ResourceAccess::UavReadWriteComp:
    case ResourceAccess::UavReadWriteGr:
    case ResourceAccess::UavReadWriteAll:
      break;
    }
  if(rs==(ResourceAccess::TransferSrc|ResourceAccess::TransferDst))
    return "Transfer";
  return "Uav";
  }

struct TestTexture : Tempest::AbstractGraphicsApi::Texture {
  uint32_t mipCount() const override { return 1; }
  };

struct TestCommandBuffer : Tempest::AbstractGraphicsApi::CommandBuffer {
  void beginRendering(const AttachmentDesc* desc, size_t descSize,
                      uint32_t w, uint32_t h,
                      const TextureFormat* frm,
                      AbstractGraphicsApi::Texture** att,
                      AbstractGraphicsApi::Swapchain** sw, const uint32_t* imgId) override {}
  void endRendering() override {}

  void barrier(const AbstractGraphicsApi::BarrierDesc* desc, size_t cnt) override;

  void generateMipmap(AbstractGraphicsApi::Texture& image, uint32_t texWidth, uint32_t texHeight, uint32_t mipLevels) override {}
  void copy(AbstractGraphicsApi::Buffer& dest, size_t offset, AbstractGraphicsApi::Texture& src, uint32_t width, uint32_t height, uint32_t mip) override {}

  bool isRecording() const override { return true; }
  void begin() override {}
  void end() override {}
  void reset() override {}

  void setPipeline(AbstractGraphicsApi::Pipeline& p) override {}
  void setComputePipeline(AbstractGraphicsApi::CompPipeline& p) override {}

  void setBytes   (AbstractGraphicsApi::Pipeline &p, const void* data, size_t size) override {}
  void setUniforms(AbstractGraphicsApi::Pipeline& p, AbstractGraphicsApi::Desc& u) override {}

  void setBytes   (AbstractGraphicsApi::CompPipeline &p, const void* data, size_t size) override {}
  void setUniforms(AbstractGraphicsApi::CompPipeline& p, AbstractGraphicsApi::Desc& u) override {}

  void setViewport(const Rect& r) override {}
  void setScissor (const Rect& r) override {}
  void setDebugMarker(std::string_view tag) override {}

  void draw        (size_t vsize, size_t firstInstance, size_t instanceCount) override {}
  void draw        (const AbstractGraphicsApi::Buffer& vbo, size_t stride, size_t offset, size_t vertexCount,
            size_t firstInstance, size_t instanceCount) override {}
  void drawIndexed (const AbstractGraphicsApi::Buffer& vbo, size_t stride, size_t voffset,
                    const AbstractGraphicsApi::Buffer& ibo, Detail::IndexClass cls, size_t ioffset, size_t isize,
                   size_t firstInstance, size_t instanceCount) override {}
  void dispatch    (size_t x, size_t y, size_t z) override {}
  };

void TestCommandBuffer::barrier(const AbstractGraphicsApi::BarrierDesc* desc, size_t cnt) {
  for(size_t i=0; i<cnt; ++i) {
    auto& d    = desc[i];
    auto  prev = toString(d.prev);
    if(d.discard)
      prev = "Discard";
    Log::d("barrier {", prev, " -> ", toString(d.next), "}");
    }
  }

TEST(main, ResourceStateBasic) {
  TestTexture       t;
  TestCommandBuffer cmd;

  ResourceState rs;
  rs.setLayout(t, ResourceAccess::ColorAttach, true);
  rs.flush(cmd);
  rs.setLayout(t, ResourceAccess::Sampler, false);
  rs.flush(cmd);
  }

TEST(main, ResourceStateTranfer) {
  TestCommandBuffer cmd;

  ResourceState rs;
  rs.onTranferUsage(NonUniqResId::I_None, NonUniqResId::I_Ssbo, false);
  //rs.flush(cmd);
  rs.finalize(cmd);
/*
  rs.onTranferUsage(NonUniqResId::I_None, NonUniqResId::I_Ssbo);
  rs.flush(cmd);

  rs.joinCompute(PipelineStage::S_Graphics);

  ResourceState::Usage u = {NonUniqResId::I_Ssbo, NonUniqResId::I_None, false};
  rs.onUavUsage(u, PipelineStage::S_Compute);
  rs.flush(cmd);
  */
  }



