#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <d3d12.h>
#include "gapi/directx12/comptr.h"

namespace Tempest {
namespace Detail {

class DxDevice;

class DxShader:public AbstractGraphicsApi::Shader {
  public:
    DxShader(const void* source, size_t src_size);
    ~DxShader();

    D3D12_SHADER_BYTECODE bytecode();
    ComPtr<ID3DBlob> shader;
  };

}}
