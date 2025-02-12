#pragma once

#include "gapi/abstractgraphicsapi.h"
#include "gapi/shaderreflection.h"
#include "gapi/directx12/dxdescriptorallocator.h"

#include <d3d12.h>

namespace Tempest {
namespace Detail {

class DxDevice;

class DxPushDescriptor {
  public:
    using Bindings   = Detail::Bindings;
    // using WriteInfo  = VBindlessCache::WriteInfo;
    using PushBlock  = ShaderReflection::PushBlock;
    using LayoutDesc = ShaderReflection::LayoutDesc;

    struct DescSet {
      D3D12_GPU_DESCRIPTOR_HANDLE res = {};
      D3D12_GPU_DESCRIPTOR_HANDLE smp = {};
      };

    DxPushDescriptor(DxDevice& dev);
    ~DxPushDescriptor();
    void reset();

    uint32_t                     alloc(uint32_t numRes);
    std::pair<uint32_t,uint32_t> alloc(uint32_t numRes, uint32_t numSmp);

    DescSet push(const PushBlock &pb, const LayoutDesc& lay, const Bindings& binding);
    void    setRootTable(ID3D12GraphicsCommandList& enc, const DescSet& set,
                         const LayoutDesc& lay, const Bindings& binding, const PipelineStage st);

    static  void write(DxDevice& dev, D3D12_CPU_DESCRIPTOR_HANDLE res, D3D12_CPU_DESCRIPTOR_HANDLE s,
                       ShaderReflection::Class cls, AbstractGraphicsApi::NoCopy* data, uint32_t offset,
                       const ComponentMapping& map, const Sampler& smp);
    static  void write(DxDevice& dev, D3D12_CPU_DESCRIPTOR_HANDLE res, const Sampler& smp);

  private:
    DxDevice& dev;

    enum {
      RES_ALLOC_SZ = 256>MaxBindings ? 256 : MaxBindings,
      SMP_ALLOC_SZ = MaxBindings,
      };

    template<D3D12_DESCRIPTOR_HEAP_TYPE T>
    struct Pool {
      Pool(DxDevice& dev, uint32_t size);

      uint32_t                   dPtr  = 0;
      uint32_t                   alloc = 0;
      };

    template<D3D12_DESCRIPTOR_HEAP_TYPE T>
    uint32_t                     alloc(std::vector<Pool<T>>& resPool, const uint32_t sz, const uint32_t step);

    void setRootDescriptorTable(ID3D12GraphicsCommandList& enc, const PipelineStage st, UINT RootParameterIndex, D3D12_GPU_DESCRIPTOR_HANDLE BaseDescriptor);

    using ResPool = Pool<D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV>;
    using SmpPool = Pool<D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER>;

    std::vector<ResPool> resPool;
    std::vector<SmpPool> smpPool;
  };

}
}
