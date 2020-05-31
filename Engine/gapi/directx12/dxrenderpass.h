#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <Tempest/RenderPass>

#include <cstdint>
#include <vector>

#include <d3d12.h>
#include "gapi/directx12/comptr.h"

namespace Tempest {

namespace Detail {

class DxDevice;

class DxRenderPass : public AbstractGraphicsApi::Pass {
  public:
    DxRenderPass(const FboMode** att, size_t acount);

    std::vector<FboMode> att;
  };

}
}
