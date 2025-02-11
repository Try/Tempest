#if defined(TEMPEST_BUILD_VULKAN)
#include "vpipelinelay.h"

#include <Tempest/PipelineLayout>
#include <cstring>

#include "vdevice.h"
#include "vpipeline.h"
#include "gapi/shaderreflection.h"

using namespace Tempest;
using namespace Tempest::Detail;

VPipelineLay::VPipelineLay(VDevice& dev, const std::vector<ShaderReflection::Binding>* sh)
  : VPipelineLay(dev,&sh,1) {
  }

VPipelineLay::VPipelineLay(VDevice& dev, const std::vector<ShaderReflection::Binding>* sh[], size_t cnt) {
  setupLayout(pb, layout, sync, sh, cnt);
  dLay = dev.setLayouts.findLayout(layout);
  }

VPipelineLay::~VPipelineLay() {
  }

size_t VPipelineLay::sizeofBuffer(size_t layoutBind, size_t arraylen) const {
  return layout.sizeofBuffer(layoutBind, arraylen);
  }

void VPipelineLay::setupLayout(PushBlock& pb, LayoutDesc& lx, SyncDesc& sync,
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

      lx.bufferSz[e.layout] = std::max(lx.bufferSz[e.layout], e.byteSize);
      lx.bufferEl[e.layout] = std::max(lx.bufferEl[e.layout], e.varByteSize);
      }
    }
  }

#endif

