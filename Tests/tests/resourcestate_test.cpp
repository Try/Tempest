#include "../gapi/resourcestate.h"

#include <Tempest/Log>

#include <gtest/gtest.h>
#include <gmock/gmock-matchers.h>
#include <sstream>

using namespace testing;

using namespace Tempest;
using namespace Tempest::Detail;

static std::string toString(ResourceAccess rs) {
  std::stringstream text;
  if((rs & ResourceAccess::TransferSrc)==ResourceAccess::TransferSrc)
    text << "TransferSrc | ";
  if((rs & ResourceAccess::TransferDst)==ResourceAccess::TransferDst)
    text << "TransferDst | ";
  if((rs & ResourceAccess::TransferHost)==ResourceAccess::TransferHost)
    text << "TransferHost | ";
  if((rs & ResourceAccess::Present)==ResourceAccess::Present)
    text << "Present | ";
  if((rs & ResourceAccess::Sampler)==ResourceAccess::Sampler)
    text << "Sampler | ";
  if((rs & ResourceAccess::ColorAttach)==ResourceAccess::ColorAttach)
    text << "ColorAttach | ";
  if((rs & ResourceAccess::DepthAttach)==ResourceAccess::DepthAttach)
    text << "DepthAttach | ";
  if((rs & ResourceAccess::Index)==ResourceAccess::Index)
    text << "Index | ";
  if((rs & ResourceAccess::Vertex)==ResourceAccess::Vertex)
    text << "Vertex | ";
  if((rs & ResourceAccess::Uniform)==ResourceAccess::Uniform)
    text << "Uniform | ";
  if((rs & ResourceAccess::UavReadComp)==ResourceAccess::UavReadComp)
    text << "UavReadComp | ";
  if((rs & ResourceAccess::UavWriteComp)==ResourceAccess::UavWriteComp)
    text << "UavWriteComp | ";
  if((rs & ResourceAccess::UavReadGr)==ResourceAccess::UavReadGr)
    text << "UavReadGr | ";
  if((rs & ResourceAccess::UavWriteGr)==ResourceAccess::UavWriteGr)
    text << "UavWriteGr | ";
  if((rs & ResourceAccess::RtAsRead)==ResourceAccess::RtAsRead)
    text << "RtAsRead | ";
  if((rs & ResourceAccess::RtAsWrite)==ResourceAccess::RtAsWrite)
    text << "RtAsWrite | ";
  if ((rs & ResourceAccess::Indirect) == ResourceAccess::Indirect)
    text << "Indirect | ";


  auto ret = text.str();
  if(ret.rfind(" | ")==ret.size()-3)
    ret.resize(ret.size()-3);
  return ret;
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

  void setPushData(const void* data, size_t size) override {}
  void setBinding (size_t id, AbstractGraphicsApi::Texture *tex, uint32_t mipLevel, const ComponentMapping&, const Sampler& smp) override {}
  void setBinding (size_t id, AbstractGraphicsApi::Buffer* buf, size_t offset) override {}
  void setBinding (size_t id, AbstractGraphicsApi::DescArray* arr) override {}
  void setBinding (size_t id, AbstractGraphicsApi::AccelerationStructure* tlas) override {}
  void setBinding (size_t id, const Sampler& smp) override {}

  void setViewport(const Rect& r) override {}
  void setScissor (const Rect& r) override {}
  void setDebugMarker(std::string_view tag) override {}

  void draw        (const AbstractGraphicsApi::Buffer* vbo, size_t stride, size_t offset, size_t vertexCount,
            size_t firstInstance, size_t instanceCount) override {}
  void drawIndexed (const AbstractGraphicsApi::Buffer* vbo, size_t stride, size_t voffset,
                    const AbstractGraphicsApi::Buffer& ibo, Detail::IndexClass cls, size_t ioffset, size_t isize,
                   size_t firstInstance, size_t instanceCount) override {}
  void drawIndirect(const AbstractGraphicsApi::Buffer& indirect, size_t offset) override {}

  void dispatch    (size_t x, size_t y, size_t z) override {}
  void dispatchIndirect(const AbstractGraphicsApi::Buffer& indirect, size_t offset) override {}
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

TEST(main, ResourceStateJoin) {
  TestTexture       t;
  TestCommandBuffer cmd;

  {
    ResourceState rs;
    rs.onUavUsage(NonUniqResId::I_None, NonUniqResId(0x1), PipelineStage::S_Compute);
    rs.flush(cmd);

    rs.joinWriters(PipelineStage::S_Indirect);
    rs.joinWriters(PipelineStage::S_Graphics);
    rs.flush(cmd);
  }
  {
    ResourceState rs;
    rs.onUavUsage(NonUniqResId::I_None, NonUniqResId(0x1), PipelineStage::S_RtAs);
    rs.flush(cmd);

    rs.joinWriters(PipelineStage::S_Indirect);
    rs.joinWriters(PipelineStage::S_Graphics);
    rs.flush(cmd);
    rs.onUavUsage(NonUniqResId(0x1), NonUniqResId::I_None, PipelineStage::S_Graphics);

    rs.joinWriters(PipelineStage::S_Indirect);
    rs.joinWriters(PipelineStage::S_Graphics);
    rs.flush(cmd);
  }
  }

TEST(main, ResourceStateTranfer) {
  TestCommandBuffer cmd;

  ResourceState rs;
  rs.onTranferUsage(NonUniqResId::I_None, NonUniqResId(0x1), false);
  rs.flush(cmd);

  rs.onTranferUsage(NonUniqResId::I_None, NonUniqResId(0x1), false);
  rs.flush(cmd);

  rs.onUavUsage(NonUniqResId(0x1), NonUniqResId::I_None, PipelineStage::S_Compute);
  rs.flush(cmd);
  }

TEST(main, ResourceStateBlas) {
  TestCommandBuffer cmd;

  ResourceState rs;

  rs.onUavUsage(NonUniqResId::I_None, NonUniqResId(0x1), PipelineStage::S_RtAs);
  rs.flush(cmd);

  rs.onUavUsage(NonUniqResId(0x1), NonUniqResId::I_None, PipelineStage::S_Compute);
  rs.flush(cmd);
  }

TEST(main, ResourceStateIndirect) {
  TestCommandBuffer cmd;

  ResourceState rs;

  rs.onUavUsage(NonUniqResId::I_None, NonUniqResId(0x1), PipelineStage::S_Compute);
  rs.flush(cmd);

  rs.onUavUsage(NonUniqResId(0x1), NonUniqResId::I_None, PipelineStage::S_Indirect);
  rs.flush(cmd);
  }

TEST(main, ResourceStateIndirectAndUAVWithSubsequentWriteAccess) {
  TestCommandBuffer cmd;

  ResourceState rs;

  rs.onUavUsage(NonUniqResId::I_None, NonUniqResId(0x1), PipelineStage::S_Compute);
  rs.flush(cmd);

  rs.onUavUsage(NonUniqResId(0x1), NonUniqResId::I_None, PipelineStage::S_Compute);
  rs.onUavUsage(NonUniqResId(0x1), NonUniqResId::I_None, PipelineStage::S_Indirect);
  rs.flush(cmd);

  rs.onUavUsage(NonUniqResId::I_None, NonUniqResId(0x1), PipelineStage::S_Compute);
  rs.flush(cmd);
  }



