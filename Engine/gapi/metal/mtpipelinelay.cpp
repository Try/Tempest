#if defined(TEMPEST_BUILD_METAL)

#include "mtpipelinelay.h"

#include "mtshader.h"

using namespace Tempest;
using namespace Tempest::Detail;

MtPipelineLay::MtPipelineLay(const std::vector<Binding>** sh, size_t cnt, ShaderReflection::Stage bufferSizeBuffer)
  : bufferSizeBuffer(bufferSizeBuffer) {
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

size_t MtPipelineLay::sizeofBuffer(size_t layoutBind, size_t arraylen) const {
  return ShaderReflection::sizeofBuffer(lay[layoutBind], arraylen);
  }

#endif

