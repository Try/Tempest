#include "mtpipelinelay.h"

#include "mtshader.h"

using namespace Tempest;
using namespace Tempest::Detail;

MtPipelineLay::MtPipelineLay(const std::vector<Binding>** sh, size_t cnt) {
  ShaderReflection::merge(lay,pb,sh,cnt);
  bind.resize(lay.size());

  for(size_t i=0; i<cnt; ++i) {
    if(sh[i]==nullptr)
      continue;
    for(auto& b:*sh[i]) {
      auto& bx = (b.cls==ShaderReflection::Push) ? bindPush : bind[b.layout];
      if((b.stage & vertMask)!=0)
        bx.bindVs = b.mslBinding;
      if((b.stage & ShaderReflection::Fragment)!=0)
        bx.bindFs = b.mslBinding;
      if((b.stage & ShaderReflection::Compute)!=0)
        bx.bindCs = b.mslBinding;
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
    if(i.size==0)
      i.size = 0; // TODO: ???
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

