#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <d3d12.h>

#include "gapi/directx12/comptr.h"

namespace Tempest {
namespace Detail {

class DxDevice;

class DxShader:public AbstractGraphicsApi::Shader {
  public:
    DxShader(DxDevice& device, const void* source, size_t src_size);
    ~DxShader();

    ComPtr<ID3DBlob> shader;

  private:
    ID3D12Device* device=nullptr;
  };

}}
