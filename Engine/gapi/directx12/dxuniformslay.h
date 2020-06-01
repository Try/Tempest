#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <vector>

#include <d3d12.h>
#include "gapi/directx12/comptr.h"

namespace Tempest {

class UniformsLayout;

namespace Detail {

class DxDescriptorArray;
class DxDevice;

class DxUniformsLay : public AbstractGraphicsApi::UniformsLay {
  public:
    DxUniformsLay(DxDevice& device, const UniformsLayout& lay);

    struct Param {
      UINT    heapOffset;
      uint8_t heapId=0;
      };
    std::vector<Param>          prm;
    ComPtr<ID3D12RootSignature> impl;
  };

}
}

