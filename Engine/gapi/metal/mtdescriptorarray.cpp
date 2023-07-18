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
  d.argsBuf = MtBuffer(dev, nullptr, cnt*sizeof(uint64_t), MTL::ResourceStorageModePrivate);

  std::unique_ptr<uint64_t[]> addr(new uint64_t[cnt]);
  d.args.reserve(cnt);
  for(size_t i=0; i<cnt; ++i) {
    if(buf[i]==nullptr) {
      addr[i] = 0;
      continue;
      }
    addr[i]   = reinterpret_cast<MtBuffer*>(buf[i])->impl->gpuAddress();
    d.args.push_back(reinterpret_cast<MtBuffer*>(buf[i])->impl.get());
    }
  d.argsBuf.update(addr.get(), 0, cnt*sizeof(uint64_t));
  }

void MtDescriptorArray::fillBufferSizeBuffer(uint32_t* ret, ShaderReflection::Stage stage) {
  auto& lx  = lay.handler->lay;
  auto& mtl = lay.handler->bind;
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
      auto& b = *reinterpret_cast<MTL::Buffer*>(desc[i].val);
      ret[at] = b.length() - desc[i].offset;
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
