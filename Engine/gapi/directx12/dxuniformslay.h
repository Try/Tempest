#pragma once

#include <Tempest/AbstractGraphicsApi>

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

    ComPtr<ID3D12RootSignature> impl;
  };

}
}

