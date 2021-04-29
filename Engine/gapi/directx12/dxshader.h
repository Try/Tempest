#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <Tempest/PipelineLayout>

#include <d3d12.h>
#include "gapi/shaderreflection.h"
#include "gapi/directx12/comptr.h"

namespace Tempest {
namespace Detail {

class DxDevice;

class DxShader:public AbstractGraphicsApi::Shader {
  public:
    DxShader(const void* source, size_t src_size);
    ~DxShader();

    using Binding = ShaderReflection::Binding;

    D3D12_SHADER_BYTECODE    bytecode() const;
    void                     disasm() const;

    std::vector<Decl::ComponentType> vdecl;
    std::vector<Binding>             lay;

  private:
    mutable ComPtr<ID3DBlob>         shader;
  };

}}
