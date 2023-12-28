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

MtDescriptorArray::MtDescriptorArray(MtDevice& dev, const MtPipelineLay &lay)
  :dev(dev), lay(&lay), desc(lay.lay.size()), bufferSizeBuffer(lay.bufferSizeBuffer) {
  }

void MtDescriptorArray::set(size_t id, AbstractGraphicsApi::Texture *tex, const Sampler &smp, uint32_t mipLevel) {
  auto& t = *reinterpret_cast<MtTexture*>(tex);
  desc[id].val     = &t.view(smp.mapping,mipLevel);
  desc[id].sampler = &dev.samplers.get(smp);
  }

void MtDescriptorArray::set(size_t id, const Sampler& smp) {
  desc[id].val     = nullptr;
  desc[id].sampler = &dev.samplers.get(smp);
  }

void MtDescriptorArray::set(size_t id, AbstractGraphicsApi::Buffer *buf, size_t offset) {
  auto& b = *reinterpret_cast<MtBuffer*>(buf);
  desc[id].val    = b.impl.get();
  desc[id].offset = offset;
  desc[id].length = b.size - offset;
  }

void MtDescriptorArray::setTlas(size_t id, AbstractGraphicsApi::AccelerationStructure* a) {
  auto& as = *reinterpret_cast<MtTopAccelerationStructure*>(a);
  desc[id].val  = as.impl.get();
  desc[id].tlas = &as;
  }

void MtDescriptorArray::set(size_t id, AbstractGraphicsApi::Texture** tex, size_t cnt,
                            const Sampler& smp, uint32_t mipLevel) {
  auto& d   = desc[id];
  auto  cls = lay.handler->lay[id].cls;

  size_t shift = 0;
  size_t bufSz = cnt*sizeof(MTL::ResourceID);
  if(cls==ShaderReflection::Texture) {
    bufSz  = ((bufSz+16-1)/16)*16;
    shift  = bufSz;
    bufSz += cnt*sizeof(MTL::ResourceID);
    }
  d.argsBuf = MtBuffer(dev, nullptr, bufSz, MTL::ResourceStorageModePrivate);

  std::unique_ptr<uint8_t[]> addr(new uint8_t[bufSz]);
  d.args.reserve(cnt);
  for(size_t i=0; i<cnt; ++i) {
    auto* ptr = reinterpret_cast<MTL::ResourceID*>(addr.get());
    if(tex[i]==nullptr) {
      ptr[i] = MTL::ResourceID{0};
      continue;
      }
    auto& t    = *reinterpret_cast<MtTexture*>(tex[i]);
    auto& view = t.view(smp.mapping,mipLevel);
    ptr[i]     = view.gpuResourceID();
    d.args.push_back(reinterpret_cast<MtTexture*>(tex[i])->impl.get());
    }

  if(cls==ShaderReflection::Texture) {
    auto& sampler = dev.samplers.get(smp,true);
    auto* ptr     = reinterpret_cast<MTL::ResourceID*>(addr.get() + shift);
    for(size_t i=0; i<cnt; ++i) {
      ptr[i] = sampler.gpuResourceID();
      }
    }

  d.argsBuf.update(addr.get(), 0, bufSz);
  }

void MtDescriptorArray::set(size_t id, AbstractGraphicsApi::Buffer** buf, size_t cnt) {
  auto& d = desc[id];

  struct spvBufferDescriptor {
    uint64_t ptr;
    uint32_t len;
    // 4 bytes of padding
    };
  d.argsBuf = MtBuffer(dev, nullptr, cnt*sizeof(spvBufferDescriptor), MTL::ResourceStorageModePrivate);

  std::unique_ptr<spvBufferDescriptor[]> addr(new spvBufferDescriptor[cnt]);
  d.args.reserve(cnt);
  for(size_t i=0; i<cnt; ++i) {
    if(buf[i]==nullptr) {
      addr[i].ptr = 0;
      addr[i].len = 0;
      continue;
      }
    addr[i].ptr = reinterpret_cast<MtBuffer*>(buf[i])->impl->gpuAddress();
    addr[i].len = uint32_t(reinterpret_cast<MtBuffer*>(buf[i])->size);
    d.args.push_back(reinterpret_cast<MtBuffer*>(buf[i])->impl.get());
    }
  d.argsBuf.update(addr.get(), 0, cnt*sizeof(spvBufferDescriptor));
  }

void MtDescriptorArray::fillBufferSizeBuffer(uint32_t* ret, ShaderReflection::Stage stage, const MtPipelineLay& lay) {
  auto& lx  = lay.lay;
  auto& mtl = lay.bind;
  for(size_t i=0; i<lx.size(); ++i) {
    if(lx[i].cls==ShaderReflection::SsboR ||
       lx[i].cls==ShaderReflection::SsboRW) {
      uint32_t at = uint32_t(-1);
      switch(stage) {
        case ShaderReflection::None:
        case ShaderReflection::Control:
        case ShaderReflection::Evaluate:
        case ShaderReflection::Geometry:
          break;
        case ShaderReflection::Task:
          at = mtl[i].bindTs;
          break;
        case ShaderReflection::Mesh:
          at = mtl[i].bindMs;
          break;
        case ShaderReflection::Vertex:
          at = mtl[i].bindVs;
          break;
        case ShaderReflection::Fragment:
          at = mtl[i].bindFs;
          break;
        case ShaderReflection::Compute:
          at = mtl[i].bindCs;
          break;
        }
      if(at==uint32_t(-1))
        continue;
      ret[at] = uint32_t(desc[i].length);
      }
    }
  }

void MtDescriptorArray::useResource(MTL::ComputeCommandEncoder& cmd) {
  implUseResource(cmd);
  }

void MtDescriptorArray::useResource(MTL::RenderCommandEncoder& cmd) {
  implUseResource(cmd);
  }

template<class Enc>
void MtDescriptorArray::implUseResource(Enc&  cmd) {
  auto& lx  = lay.handler->lay;
  auto& mtl = lay.handler->bind;
  for(size_t i=0; i<lx.size(); ++i) {
    if(lx[i].cls==ShaderReflection::Tlas) {
      auto& blas = desc[i].tlas->blas;
      cmd.useResources(blas.data(), blas.size(), MTL::ResourceUsageRead);
      }
    if(lx[i].runtimeSized) {
      auto& args = desc[i].args;
      cmd.useResources(args.data(), args.size(), MTL::ResourceUsageRead | MTL::ResourceUsageSample);
      }
    }
  }

#endif
