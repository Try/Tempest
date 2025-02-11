#pragma once

#include <Tempest/AbstractGraphicsApi>

#include "gapi/shader.h"
#include "gapi/directx12/comptr.h"

#include <d3d12.h>

namespace Tempest {
namespace Detail {

class DxShader : public Tempest::Detail::Shader {
  public:
    DxShader(const void* source, const size_t src_size);
    ~DxShader();

    enum Bindings : uint32_t {
      HLSL_PUSH                 = 256,
      HLSL_BASE_VERTEX_INSTANCE = 257,
      };

    D3D12_SHADER_BYTECODE    bytecode() const;

  private:
    HRESULT                  compile(ComPtr<ID3DBlob>& shader, const char* hlsl, size_t len, spv::ExecutionModel exec, uint32_t sm) const;
    mutable ComPtr<ID3DBlob> shader;
  };

}}
