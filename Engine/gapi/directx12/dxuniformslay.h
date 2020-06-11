#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <Tempest/UniformsLayout>
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
    DxUniformsLay(DxDevice& device, const std::vector<UniformsLayout::Binding>& vs, const std::vector<UniformsLayout::Binding>& fs);

    enum VisType {
      Vertex   = 0,
      Fragment = 1,
      VisTypeCount,
      };

    enum {
      MaxPrmPerStage = 2,
      };

    using Binding = UniformsLayout::Binding;

    struct Param {
      UINT    heapOffset;
      uint8_t heapId=0;

      UINT    heapOffsetSmp;
      uint8_t heapIdSmp=0;
      };

    struct Heap {
      D3D12_DESCRIPTOR_HEAP_TYPE type    = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
      UINT                       numDesc = 0;
      };

    std::vector<Param>          prm;
    std::vector<Heap>           heaps;

    ComPtr<ID3D12RootSignature> impl;
    UINT                        descSize = 0;
    std::vector<Binding>        lay;

  private:
    void add(const UniformsLayout::Binding& b, D3D12_DESCRIPTOR_RANGE_TYPE type, Param& prm,
             D3D12_ROOT_PARAMETER* root, D3D12_DESCRIPTOR_RANGE* rgn, size_t& cnt);
  };

}
}

