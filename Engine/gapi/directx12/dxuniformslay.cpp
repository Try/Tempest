#include "dxuniformslay.h"

#include "dxdevice.h"
#include "guid.h"

using namespace Tempest;
using namespace Tempest::Detail;

DxUniformsLay::DxUniformsLay(DxDevice& dev, const UniformsLayout& lay) {
  auto& device=*dev.device;

  D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};
  featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;

  D3D12_ROOT_PARAMETER rootParameters[1]={};
  rootParameters[0].ParameterType    = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
  rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

  /*
  D3D12_STATIC_SAMPLER_DESC sampler = {};
  sampler.Filter           = D3D12_FILTER_MIN_MAG_MIP_POINT;
  sampler.AddressU         = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
  sampler.AddressV         = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
  sampler.AddressW         = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
  sampler.MipLODBias       = 0;
  sampler.MaxAnisotropy    = 0;
  sampler.ComparisonFunc   = D3D12_COMPARISON_FUNC_NEVER;
  sampler.BorderColor      = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
  sampler.MinLOD           = 0.0f;
  sampler.MaxLOD           = D3D12_FLOAT32_MAX;
  sampler.ShaderRegister   = 0;
  sampler.RegisterSpace    = 0;
  sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
  */

  D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc={};
  rootSignatureDesc.NumParameters     = 0;//1;//_countof(rootParameters);
  rootSignatureDesc.pParameters       = nullptr;//rootParameters;
  rootSignatureDesc.NumStaticSamplers = 0;
  rootSignatureDesc.pStaticSamplers   = nullptr;//&sampler;
  rootSignatureDesc.Flags             = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

  ComPtr<ID3DBlob> signature;
  ComPtr<ID3DBlob> error;

  dxAssert(D3D12SerializeRootSignature(&rootSignatureDesc, featureData.HighestVersion,
                                       &signature.get(), &error.get()));
  dxAssert(device.CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(),
                                      uuid<ID3D12RootSignature>(), reinterpret_cast<void**>(&impl)));
  }
