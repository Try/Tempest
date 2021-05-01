#include "mtpipelinelay.h"

#include "mtshader.h"

using namespace Tempest;
using namespace Tempest::Detail;

MtPipelineLay::MtPipelineLay(const std::vector<Binding> &cs) {
  ShaderReflection::merge(lay,pb,cs);
  adjustSsboBindings();
  }

MtPipelineLay::MtPipelineLay(const std::vector<Binding>** sh, size_t cnt) {
  ShaderReflection::merge(lay,pb,sh,cnt);
  adjustSsboBindings();
  }

size_t MtPipelineLay::descriptorsCount() {
  return lay.size();
  }

void MtPipelineLay::adjustSsboBindings() {
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
