#pragma once

#include <Tempest/AbstractGraphicsApi>
#include "nsptr.h"

#import  <Metal/MTLRenderCommandEncoder.h>

namespace Tempest {
namespace Detail {

class MtPipeline : public AbstractGraphicsApi::Pipeline {
  public:
    MtPipeline(const Tempest::RenderState& rs,
               size_t stride,
               const NsPtr& vert,
               const NsPtr& frag);

    struct Inst {

      };

  private:
    uint32_t         stride   = 0;
    MTLPrimitiveType topology = MTLPrimitiveTypeTriangle;

    NsPtr vert;
    NsPtr frag;
  };

}
}

