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
    DxShader(const void* source, const size_t src_size);
    ~DxShader();

    enum Bindings : uint32_t {
      HLSL_PUSH                 = 256,
      HLSL_BASE_VERTEX_INSTANCE = 257,
    };

    using Binding = ShaderReflection::Binding;

    D3D12_SHADER_BYTECODE    bytecode() const;

    std::vector<Decl::ComponentType> vdecl;
    std::vector<Binding>             lay;
    ShaderReflection::Stage          stage = ShaderReflection::Stage::Compute;
    bool                             has_baseVertex_baseInstance = false;
    bool                             has_NumworkGroups = false;

    struct Comp {
      IVec3 wgSize;
      } comp;

  private:
    HRESULT                          compile(ComPtr<ID3DBlob>& shader, const char* hlsl, size_t len, spv::ExecutionModel exec, uint32_t sm) const;
    mutable ComPtr<ID3DBlob>         shader;
  };

}}
