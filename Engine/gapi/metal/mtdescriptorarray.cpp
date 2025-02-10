#if defined(TEMPEST_BUILD_METAL)

#include "mtdescriptorarray.h"

#include "mtdevice.h"
#include "mttexture.h"
#include "mtbuffer.h"
#include "mtpipelinelay.h"
#include "mtaccelerationstructure.h"
#include "mtshader.h"

using namespace Tempest;
using namespace Tempest::Detail;

MtDescriptorArray2::MtDescriptorArray2(MtDevice& dev, AbstractGraphicsApi::Texture** tex, size_t cnt,
                                       uint32_t mipLevel, const Sampler& smp)
  : cnt(cnt) {
  fillBuffer(dev, tex, cnt, mipLevel, &smp);
  }

MtDescriptorArray2::MtDescriptorArray2(MtDevice& dev, AbstractGraphicsApi::Texture** tex, size_t cnt, uint32_t mipLevel)
  : cnt(cnt) {
  fillBuffer(dev, tex, cnt, mipLevel, nullptr);
  }

MtDescriptorArray2::MtDescriptorArray2(MtDevice& dev, AbstractGraphicsApi::Buffer** buf, size_t cnt)
  : cnt(cnt) {
  fillBuffer(dev, buf, cnt);
  }

MtDescriptorArray2::~MtDescriptorArray2() {
  }

size_t MtDescriptorArray2::size() const {
  return cnt;
  }

void MtDescriptorArray2::fillBuffer(MtDevice& dev, AbstractGraphicsApi::Texture** tex, size_t cnt,
                                    uint32_t mipLevel, const Sampler* smp) {
  const bool combined = false;

  size_t shift = 0;
  size_t bufSz = cnt*sizeof(MTL::ResourceID);
  if(combined) {
    bufSz  = ((bufSz+16-1)/16)*16;
    shift  = bufSz;
    bufSz += cnt*sizeof(MTL::ResourceID);
    }
  argsBuf = MtBuffer(dev, nullptr, bufSz, MTL::ResourceStorageModePrivate);

  std::unique_ptr<uint8_t[]> addr(new uint8_t[bufSz]);
  args.reserve(cnt);
  args.clear();
  for(size_t i=0; i<cnt; ++i) {
    auto* ptr = reinterpret_cast<MTL::ResourceID*>(addr.get());
    if(tex[i]==nullptr) {
      ptr[i] = MTL::ResourceID{0};
      continue;
      }
    const auto mapping = smp!=nullptr ? smp->mapping : ComponentMapping();
    auto& t    = *reinterpret_cast<MtTexture*>(tex[i]);
    auto& view = t.view(mapping,mipLevel);
    ptr[i]     = view.gpuResourceID();
    args.push_back(reinterpret_cast<MtTexture*>(tex[i])->impl.get());
    }

  if(combined) {
    auto& sampler = dev.samplers.get(*smp,true);
    auto* ptr     = reinterpret_cast<MTL::ResourceID*>(addr.get() + shift);
    for(size_t i=0; i<cnt; ++i) {
      ptr[i] = sampler.gpuResourceID();
      }
    }

  argsBuf.update(addr.get(), 0, bufSz);
  }

void MtDescriptorArray2::fillBuffer(MtDevice& dev, AbstractGraphicsApi::Buffer** buf, size_t cnt) {
  struct spvBufferDescriptor {
    uint64_t ptr;
    uint32_t len;
    // 4 bytes of padding
    };
  argsBuf = MtBuffer(dev, nullptr, cnt*sizeof(spvBufferDescriptor), MTL::ResourceStorageModePrivate);

  std::unique_ptr<spvBufferDescriptor[]> addr(new spvBufferDescriptor[cnt]);
  args.reserve(cnt);
  args.clear();
  for(size_t i=0; i<cnt; ++i) {
    if(buf[i]==nullptr) {
      addr[i].ptr = 0;
      addr[i].len = 0;
      continue;
      }
    addr[i].ptr = reinterpret_cast<MtBuffer*>(buf[i])->impl->gpuAddress();
    addr[i].len = uint32_t(reinterpret_cast<MtBuffer*>(buf[i])->size);
    args.push_back(reinterpret_cast<MtBuffer*>(buf[i])->impl.get());
    }
  argsBuf.update(addr.get(), 0, cnt*sizeof(spvBufferDescriptor));
  }

void MtDescriptorArray2::useResource(MTL::ComputeCommandEncoder& cmd, ShaderReflection::Stage st) {
  implUseResource(cmd, st);
  }

void MtDescriptorArray2::useResource(MTL::RenderCommandEncoder& cmd, ShaderReflection::Stage st) {
  implUseResource(cmd, st);
  }

template<class Enc>
void MtDescriptorArray2::implUseResource(Enc& cmd, ShaderReflection::Stage st) {
  auto stages = nativeFormat(st);
  //TODO: UsageWrite
  implUseResource(cmd, args.data(), args.size(), MTL::ResourceUsageRead | MTL::ResourceUsageSample, stages);
  }

void MtDescriptorArray2::implUseResource(MTL::ComputeCommandEncoder& cmd,
                                         const MTL::Resource* const resources[], NS::UInteger count,
                                         MTL::ResourceUsage usage, MTL::RenderStages stages) {
  (void)stages;
  cmd.useResources(resources, count, usage);
  }

void MtDescriptorArray2::implUseResource(MTL::RenderCommandEncoder& cmd,
                                         const MTL::Resource* const resources[], NS::UInteger count,
                                         MTL::ResourceUsage usage, MTL::RenderStages stages) {
  cmd.useResources(resources, count, usage, stages);
  }

#endif

