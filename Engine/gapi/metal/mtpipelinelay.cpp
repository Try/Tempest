#if defined(TEMPEST_BUILD_METAL)

#include "mtpipelinelay.h"

#include "mtshader.h"

using namespace Tempest;
using namespace Tempest::Detail;

MtPipelineLay::MtPipelineLay(const std::vector<Binding>** sh, size_t cnt, ShaderReflection::Stage bufferSizeBuffer)
  : bufferSizeBuffer(bufferSizeBuffer) {
  SyncDesc sync;
  setupLayout(pb, layout, sync, sh, cnt);

  ShaderReflection::merge(lay,pb,sh,cnt);
  bind.resize(lay.size());

  for(size_t i=0; i<cnt; ++i) {
    if(sh[i]==nullptr)
      continue;
    for(auto& b:*sh[i]) {
      auto& bx = (b.cls==ShaderReflection::Push) ? bindPush : bind[b.layout];
      if((b.stage & vertMask)!=0) {
        bx.bindVs    = b.mslBinding;
        bx.bindVsSmp = b.mslBinding2;
        }
      if((b.stage & ShaderReflection::Task)!=0) {
        bx.bindTs    = b.mslBinding;
        bx.bindTsSmp = b.mslBinding2;
        }
      if((b.stage & ShaderReflection::Mesh)!=0) {
        bx.bindMs    = b.mslBinding;
        bx.bindMsSmp = b.mslBinding2;
        }
      if((b.stage & ShaderReflection::Fragment)!=0) {
        bx.bindFs    = b.mslBinding;
        bx.bindFsSmp = b.mslBinding2;
        }
      if((b.stage & ShaderReflection::Compute)!=0) {
        bx.bindCs    = b.mslBinding;
        bx.bindCsSmp = b.mslBinding2;
        }

      if(b.cls==ShaderReflection::Sampler) {
        bx.bindVsSmp = bx.bindVs;
        bx.bindVs = -1;

        bx.bindTsSmp = bx.bindTs;
        bx.bindTs = -1;

        bx.bindMsSmp = bx.bindMs;
        bx.bindMs = -1;

        bx.bindFsSmp = bx.bindFs;
        bx.bindFs = -1;

        bx.bindCsSmp = bx.bindCs;
        bx.bindCs = -1;
        }

      bx.mslPushSize = uint32_t(b.mslSize);
      }
    }

  while(true) {
    bool done = (vboIndex!=bindPush.bindVs);
    for(auto& b:bind)
      if(b.bindVs==vboIndex)
        done = false;
    if(done)
      break;
    vboIndex++;
    }

  for(auto& i:lay)
    if(i.byteSize==0)
      i.byteSize = 0; // TODO: ???
  for(auto& i:lay)
    if(i.cls==ShaderReflection::SsboR  ||
       i.cls==ShaderReflection::SsboRW ||
       i.cls==ShaderReflection::ImgR   ||
       i.cls==ShaderReflection::ImgRW ) {
      hasSSBO = true;
      }
  }

size_t MtPipelineLay::descriptorsCount() {
  return lay.size();
  }

size_t MtPipelineLay::sizeofBuffer(size_t layoutBind, size_t arraylen) const {
  return ShaderReflection::sizeofBuffer(lay[layoutBind], arraylen);
  }

void MtPipelineLay::setupLayout(PushBlock& pb, LayoutDesc& lx, SyncDesc& sync,
                                const std::vector<Binding>* sh[], size_t cnt) {
  std::fill(std::begin(lx.bindings), std::end(lx.bindings), ShaderReflection::Count);

  for(size_t i=0; i<cnt; ++i) {
    if(sh[i]==nullptr)
      continue;
    auto& bind = *sh[i];
    for(auto& e:bind) {
      if(e.cls==ShaderReflection::Push) {
        pb.stage = ShaderReflection::Stage(pb.stage | e.stage);
        pb.size  = std::max(pb.size, e.byteSize);
        pb.size  = std::max(pb.size, e.mslSize);
        continue;
        }
      const uint32_t id = (1u << e.layout);

      lx.bindings[e.layout] = e.cls;
      lx.count   [e.layout] = std::max(lx.count[e.layout] , e.arraySize);
      lx.stage   [e.layout] = ShaderReflection::Stage(e.stage | lx.stage[e.layout]);

      if(e.runtimeSized)
        lx.runtime |= id;
      if(e.runtimeSized || e.arraySize>1)
        lx.array   |= id;
      lx.active |= id;

      sync.read |= id;
      if(e.cls==ShaderReflection::ImgRW || e.cls==ShaderReflection::SsboRW)
        sync.write |= id;

      lx.bufferSz[e.layout] = std::max<uint32_t>(lx.bufferSz[e.layout], e.byteSize);
      lx.bufferEl[e.layout] = std::max<uint32_t>(lx.bufferEl[e.layout], e.varByteSize);
      }
    }
  }

#endif

