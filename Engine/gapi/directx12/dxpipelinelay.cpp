#if defined(TEMPEST_BUILD_DIRECTX12)

#include "dxpipelinelay.h"

#include <Tempest/PipelineLayout>

#include "dxdevice.h"
#include "dxshader.h"
#include "guid.h"

#include <cassert>

using namespace Tempest;
using namespace Tempest::Detail;

static D3D12_DESCRIPTOR_RANGE_TYPE nativeFormat(ShaderReflection::Class cls) {
  switch(cls) {
    case ShaderReflection::Ubo:
      return D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
    case ShaderReflection::Texture:
      return D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // NOTE:combined
    case ShaderReflection::Image:
      return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    case ShaderReflection::Sampler:
      return D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
    case ShaderReflection::SsboR:
      return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    case ShaderReflection::SsboRW:
      return D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    case ShaderReflection::ImgR:
      return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    case ShaderReflection::ImgRW:
      return D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    case ShaderReflection::Tlas:
      return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    case ShaderReflection::Push:
    case ShaderReflection::Count:
      assert(0);
    }
  return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
  }

static D3D12_SHADER_VISIBILITY nativeFormat(ShaderReflection::Stage stage) {
  switch(stage) {
    case ShaderReflection::Stage::None:
      return D3D12_SHADER_VISIBILITY_ALL;
    case ShaderReflection::Stage::Vertex:
      return D3D12_SHADER_VISIBILITY_VERTEX;
    case ShaderReflection::Stage::Control:
      return D3D12_SHADER_VISIBILITY_HULL;
    case ShaderReflection::Stage::Evaluate:
      return D3D12_SHADER_VISIBILITY_DOMAIN;
    case ShaderReflection::Stage::Fragment:
      return D3D12_SHADER_VISIBILITY_PIXEL;
    case ShaderReflection::Stage::Geometry:
      return D3D12_SHADER_VISIBILITY_GEOMETRY;
    case ShaderReflection::Stage::Mesh:
      return D3D12_SHADER_VISIBILITY_MESH;
    case ShaderReflection::Stage::Task:
      return D3D12_SHADER_VISIBILITY_AMPLIFICATION;
    case ShaderReflection::Stage::Compute:
      return D3D12_SHADER_VISIBILITY_ALL;
    }
  return D3D12_SHADER_VISIBILITY_ALL;
  }

DxPipelineLay::DxPipelineLay(DxDevice& dev, const std::vector<ShaderReflection::Binding>* sh)
  :DxPipelineLay(dev,&sh,1,false) {
  }

DxPipelineLay::DxPipelineLay(DxDevice& dev, const std::vector<ShaderReflection::Binding>* sh[], size_t cnt,
                             bool has_baseVertex_baseInstance)
  : dev(dev) {
  setupLayout(pb, layout, sync, sh, cnt);
  init(layout, pb, has_baseVertex_baseInstance);
  }

void DxPipelineLay::setupLayout(ShaderReflection::PushBlock& pb, LayoutDesc& lx, SyncDesc& sync,
                                const std::vector<ShaderReflection::Binding>* sh[], size_t cnt) {
  //FIXME: copy-paste from vulkan
  std::fill(std::begin(lx.bindings), std::end(lx.bindings), ShaderReflection::Count);

  for(size_t i=0; i<cnt; ++i) {
    if(sh[i]==nullptr)
      continue;
    auto& bind = *sh[i];
    for(auto& e:bind) {
      if(e.cls==ShaderReflection::Push) {
        pb.stage = ShaderReflection::Stage(pb.stage | e.stage);
        pb.size  = std::max(pb.size, e.byteSize);
        pb.size  = std::max(pb.size, e.mslSize);
        continue;
        }

      const uint32_t id = (1u << e.layout);
      lx.bindings[e.layout] = e.cls;
      lx.count   [e.layout] = std::max(lx.count[e.layout] , e.arraySize);
      lx.stage   [e.layout] = ShaderReflection::Stage(e.stage | lx.stage[e.layout]);

      if(e.runtimeSized)
        lx.runtime |= id;
      if(e.isArray())
        lx.array   |= id;
      lx.active |= id;

      sync.read |= id;
      if(e.cls==ShaderReflection::ImgRW || e.cls==ShaderReflection::SsboRW)
        sync.write |= id;

      lx.bufferSz[e.layout] = std::max<uint32_t>(lx.bufferSz[e.layout], e.byteSize);
      lx.bufferEl[e.layout] = std::max<uint32_t>(lx.bufferEl[e.layout], e.varByteSize);
      }
    }
  }

void DxPipelineLay::init(const LayoutDesc& lay, const ShaderReflection::PushBlock& pb, bool has_baseVertex_baseInstance) {
  auto& device = *dev.device;

  std::vector<D3D12_DESCRIPTOR_RANGE> rgn;
  // shader-resources
  const size_t resBg  = rgn.size();
  {
    uint32_t offset = 0;
    for(size_t i=0; i<MaxBindings; ++i) {
      if(((1u << i) & lay.active)==0)
        continue;
      if(((1u << i) & lay.array)!=0)
        continue;
      if(lay.bindings[i]==ShaderReflection::Sampler)
        continue;
      D3D12_DESCRIPTOR_RANGE rx = {};
      rx.RangeType          = ::nativeFormat(lay.bindings[i]);
      rx.NumDescriptors     = 1;
      rx.BaseShaderRegister = UINT(i);
      rx.RegisterSpace      = 0;
      rx.OffsetInDescriptorsFromTableStart = UINT(offset);
      ++offset;
      rgn.push_back(rx);
      }
  }
  const size_t numRes = rgn.size() - resBg;

  // samplers
  const size_t smpBg  = rgn.size();
  {
    uint32_t offset = 0;
    for(size_t i=0; i<MaxBindings; ++i) {
      if(lay.bindings[i]!=ShaderReflection::Sampler && lay.bindings[i]!=ShaderReflection::Texture)
        continue;
      if(((1u << i) & lay.array)!=0)
        continue;
      D3D12_DESCRIPTOR_RANGE rx = {};
      rx.RangeType          = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
      rx.NumDescriptors     = 1;
      rx.BaseShaderRegister = UINT(i);
      rx.RegisterSpace      = 0;
      rx.OffsetInDescriptorsFromTableStart = UINT(offset);
      ++offset;
      rgn.push_back(rx);
      }
  }
  const size_t numSmp = rgn.size() - smpBg;

  // arrays
  const size_t arrBg  = rgn.size();
  {
    for(size_t i=0; i<MaxBindings; ++i) {
      if(((1u << i) & lay.active)==0)
        continue;
      if(((1u << i) & lay.array)==0)
        continue;
      D3D12_DESCRIPTOR_RANGE rx = {};
      rx.RangeType                         = ::nativeFormat(lay.bindings[i]);
      rx.NumDescriptors                    = -1;
      rx.BaseShaderRegister                = 0;
      rx.RegisterSpace                     = UINT(i+1);
      rx.OffsetInDescriptorsFromTableStart = 0;
      rgn.push_back(rx);
      }
  }
  const size_t numArr = rgn.size() - arrBg;

  std::vector<D3D12_ROOT_PARAMETER> rootPrm;
  if(numRes>0) {
    D3D12_ROOT_PARAMETER prm = {};
    prm.ParameterType                       = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    prm.DescriptorTable.NumDescriptorRanges = UINT(numRes);
    prm.DescriptorTable.pDescriptorRanges   = rgn.data() + resBg;
    prm.ShaderVisibility                    = D3D12_SHADER_VISIBILITY_ALL;
    rootPrm.push_back(prm);
    }
  if(numSmp>0) {
    D3D12_ROOT_PARAMETER prm = {};
    prm.ParameterType                       = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    prm.DescriptorTable.NumDescriptorRanges = UINT(numSmp);
    prm.DescriptorTable.pDescriptorRanges   = rgn.data() + smpBg;
    prm.ShaderVisibility                    = D3D12_SHADER_VISIBILITY_ALL;
    rootPrm.push_back(prm);
    }
  for(size_t i=0; i<numArr; ++i) {
    D3D12_ROOT_PARAMETER prm = {};
    prm.ParameterType                       = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    prm.DescriptorTable.NumDescriptorRanges = 1;
    prm.DescriptorTable.pDescriptorRanges   = rgn.data() + arrBg + i;
    prm.ShaderVisibility                    = D3D12_SHADER_VISIBILITY_ALL;
    rootPrm.push_back(prm);
    }
  // push
  if(pb.size>0) {
    D3D12_ROOT_PARAMETER prmPush = {};
    prmPush.ParameterType            = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    prmPush.ShaderVisibility         = ::nativeFormat(pb.stage);
    prmPush.Constants.ShaderRegister = DxShader::HLSL_PUSH;
    prmPush.Constants.RegisterSpace  = 0;
    prmPush.Constants.Num32BitValues = UINT((pb.size+3)/4);
    pushConstantId = uint32_t(rootPrm.size());
    rootPrm.push_back(prmPush);
    }
  // internal
  if(has_baseVertex_baseInstance) {
    D3D12_ROOT_PARAMETER prmPush = {};
    prmPush.ParameterType            = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    prmPush.ShaderVisibility         = D3D12_SHADER_VISIBILITY_VERTEX;
    prmPush.Constants.ShaderRegister = DxShader::HLSL_BASE_VERTEX_INSTANCE;
    prmPush.Constants.RegisterSpace  = 0;
    prmPush.Constants.Num32BitValues = 2u;
    pushBaseInstanceId = uint32_t(rootPrm.size());
    rootPrm.push_back(prmPush);
    }

  D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};
  featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;

  D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc={};
  rootSignatureDesc.NumParameters     = UINT(rootPrm.size());
  rootSignatureDesc.pParameters       = rootPrm.data();
  rootSignatureDesc.NumStaticSamplers = 0;
  rootSignatureDesc.pStaticSamplers   = nullptr;
  rootSignatureDesc.Flags             = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

  ComPtr<ID3DBlob> signature;
  ComPtr<ID3DBlob> error;

  auto hr = dev.dllApi.D3D12SerializeRootSignature(&rootSignatureDesc, featureData.HighestVersion,
                                                   &signature.get(), &error.get());
  if(FAILED(hr)) {
#if !defined(NDEBUG)
    const char* msg = reinterpret_cast<const char*>(error->GetBufferPointer());
    Log::e(msg);
#endif
    dxAssert(hr);
    }
  dxAssert(device.CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(),
                                      uuid<ID3D12RootSignature>(), reinterpret_cast<void**>(&impl)));
  }

size_t DxPipelineLay::descriptorsCount() {
  return lay.size();
  }

size_t DxPipelineLay::sizeofBuffer(size_t layoutBind, size_t arraylen) const {
  return layout.sizeofBuffer(layoutBind, arraylen);
  }


// legacy
void DxPipelineLay::init(const std::vector<Binding>& lay, const ShaderReflection::PushBlock& pb,
                         bool has_baseVertex_baseInstance) {
  auto&      device   = *dev.device;
  const UINT descSize = device.GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
  const UINT smpSize  = device.GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

  uint32_t lastBind=0;
  for(auto& i:lay)
    lastBind = std::max(lastBind,i.layout);
  if(lay.size()>0)
    prm.resize(lastBind+1);

  std::vector<Parameter> desc;
  for(size_t i=0;i<lay.size();++i) {
    auto& l = lay[i];
    if(l.stage==ShaderReflection::Stage(0))
      continue;
    switch(l.cls) {
      case ShaderReflection::Ubo: {
        add(l,D3D12_DESCRIPTOR_RANGE_TYPE_CBV,desc);
        break;
        }
      case ShaderReflection::Texture: {
        add(l,D3D12_DESCRIPTOR_RANGE_TYPE_SRV,    desc);
        add(l,D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER,desc);
        break;
        }
      case ShaderReflection::Image: {
        add(l,D3D12_DESCRIPTOR_RANGE_TYPE_SRV,    desc);
        break;
        }
      case ShaderReflection::Sampler: {
        add(l,D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER,desc);
        break;
        }
      case ShaderReflection::SsboR: {
        add(l,D3D12_DESCRIPTOR_RANGE_TYPE_SRV,desc);
        break;
        }
      case ShaderReflection::SsboRW: {
        add(l,D3D12_DESCRIPTOR_RANGE_TYPE_UAV,desc);
        break;
        }
      case ShaderReflection::ImgR: {
        add(l,D3D12_DESCRIPTOR_RANGE_TYPE_SRV,desc);
        break;
        }
      case ShaderReflection::ImgRW: {
        add(l,D3D12_DESCRIPTOR_RANGE_TYPE_UAV,desc);
        break;
        }
      case ShaderReflection::Tlas: {
        add(l,D3D12_DESCRIPTOR_RANGE_TYPE_SRV,desc);
        break;
        }
      case ShaderReflection::Push:
      case ShaderReflection::Count:
        break;
      }
    }
  std::sort(desc.begin(),desc.end(),[](const Parameter& a, const Parameter& b){
    return std::tie(a.rgn.RegisterSpace,a.visibility,a.rgn.RangeType) <
           std::tie(b.rgn.RegisterSpace,b.visibility,b.rgn.RangeType);
    });

  std::vector<D3D12_DESCRIPTOR_RANGE> rgn(desc.size());
  std::vector<D3D12_ROOT_PARAMETER>   rootPrm;

  heaps[HEAP_RES].type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
  heaps[HEAP_SMP].type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;

  uint8_t curVisibility = 255;
  uint8_t curRgnType    = 255;
  uint8_t curRgnSpace   = 255;
  for(size_t i=0; i<rgn.size(); ++i) {
    rgn[i] = desc[i].rgn;

    D3D12_DESCRIPTOR_HEAP_TYPE heapType
        = (rgn[i].RangeType==D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER) ?
          D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER :
          D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

    Heap& heap = (heapType==D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) ? heaps[HEAP_RES] : heaps[HEAP_SMP];

    if(uint8_t(desc[i].visibility)       !=curVisibility ||
       uint8_t(desc[i].rgn.RangeType)    !=curRgnType ||
       uint8_t(desc[i].rgn.RegisterSpace)!=curRgnSpace) {
      D3D12_ROOT_PARAMETER p = {};
      p.ParameterType                       = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
      p.ShaderVisibility                    = desc[i].visibility;
      p.DescriptorTable.pDescriptorRanges   = &rgn[i];
      p.DescriptorTable.NumDescriptorRanges = 0;
      rootPrm.push_back(p);

      RootPrm r = {};
      r.heap       = uint8_t(std::distance(&heaps[0],&heap));
      r.heapOffset = heap.numDesc;
      if(rgn[i].RegisterSpace>0)
        r.binding = rgn[i].RegisterSpace-1; else
        r.binding = -1;
      roots.push_back(r);

      curVisibility = uint8_t(desc[i].visibility);
      curRgnType    = uint8_t(desc[i].rgn.RangeType);
      curRgnSpace   = uint8_t(desc[i].rgn.RegisterSpace);
      }

    auto& px = rootPrm.back();
    px.DescriptorTable.NumDescriptorRanges++;
    if(rgn[i].RegisterSpace==0) {
      heap.numDesc += rgn[i].NumDescriptors;
      }

    auto& p = prm[desc[i].id];
    p.rgnType = D3D12_DESCRIPTOR_RANGE_TYPE(curRgnType);
    if(curRgnType==D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER) {
      p.heapOffsetSmp = heap.numDesc - rgn[i].NumDescriptors;
      } else {
      p.heapOffset    = heap.numDesc - rgn[i].NumDescriptors;
      }
    }

  for(size_t i=0; i<desc.size(); ++i) {
    if(desc[i].rgn.NumDescriptors!=UINT(-1))
      continue;
    size_t id = desc[i].id;
    prm[id].heapOffset    = heaps[HEAP_RES].numDesc;
    prm[id].heapOffsetSmp = heaps[HEAP_SMP].numDesc;
    }

  for(auto& i:prm) {
    i.heapOffset    *= descSize;
    i.heapOffsetSmp *= smpSize;
    }

  for(auto& i:roots) {
    if(heaps[i.heap].type==D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)
      i.heapOffset *= descSize; else
      i.heapOffset *= smpSize;
    }

  if(pb.size>0) {
    D3D12_ROOT_PARAMETER prmPush = {};
    prmPush.ParameterType            = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    prmPush.ShaderVisibility         = ::nativeFormat(pb.stage);
    prmPush.Constants.ShaderRegister = DxShader::HLSL_PUSH; //findBinding(rootPrm);
    prmPush.Constants.RegisterSpace  = 0;
    prmPush.Constants.Num32BitValues = UINT((pb.size+3)/4);
    pushConstantId = uint32_t(rootPrm.size());
    rootPrm.push_back(prmPush);
    }

  if(has_baseVertex_baseInstance) {
    D3D12_ROOT_PARAMETER prmPush = {};
    prmPush.ParameterType            = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    prmPush.ShaderVisibility         = D3D12_SHADER_VISIBILITY_VERTEX;
    prmPush.Constants.ShaderRegister = DxShader::HLSL_BASE_VERTEX_INSTANCE; //findBinding(rootPrm);
    prmPush.Constants.RegisterSpace  = 0;
    prmPush.Constants.Num32BitValues = 2u;
    pushBaseInstanceId = uint32_t(rootPrm.size());
    rootPrm.push_back(prmPush);
    }

  D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};
  featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;

  D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc={};
  rootSignatureDesc.NumParameters     = UINT(rootPrm.size());
  rootSignatureDesc.pParameters       = rootPrm.data();
  rootSignatureDesc.NumStaticSamplers = 0;
  rootSignatureDesc.pStaticSamplers   = nullptr;
  rootSignatureDesc.Flags             = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

  ComPtr<ID3DBlob> signature;
  ComPtr<ID3DBlob> error;

  auto hr = dev.dllApi.D3D12SerializeRootSignature(&rootSignatureDesc, featureData.HighestVersion,
                                                   &signature.get(), &error.get());
  if(FAILED(hr)) {
#if !defined(NDEBUG)
    const char* msg = reinterpret_cast<const char*>(error->GetBufferPointer());
    Log::e(msg);
#endif
    dxAssert(hr);
    }
  dxAssert(device.CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(),
                                      uuid<ID3D12RootSignature>(), reinterpret_cast<void**>(&impl)));
  }

void DxPipelineLay::add(const ShaderReflection::Binding& b,
                        D3D12_DESCRIPTOR_RANGE_TYPE type,
                        std::vector<Parameter>& root) {
  Parameter rp;

  rp.rgn.RangeType          = type;
  rp.rgn.NumDescriptors     = b.arraySize;
  rp.rgn.BaseShaderRegister = b.layout;
  rp.rgn.RegisterSpace      = 0;
  rp.rgn.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

  if(b.runtimeSized) {
    rp.rgn.BaseShaderRegister                = 0;
    rp.rgn.RegisterSpace                     = b.layout+1;
    rp.rgn.NumDescriptors                    = -1;
    rp.rgn.OffsetInDescriptorsFromTableStart = 0;
    }

  rp.id         = b.layout;
  rp.visibility = ::nativeFormat(b.stage);

  root.push_back(rp);
  }

void DxPipelineLay::adjustSsboBindings() {
  for(auto& i:lay)
    if(i.byteSize==0)
      ;//i.size = VK_WHOLE_SIZE; // TODO?
  }

#endif

