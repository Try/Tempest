#pragma once

#include <Tempest/AbstractGraphicsApi>

#include "dxuniformslay.h"

namespace Tempest {
namespace Detail {

class VUniformsLay;

class DxDescriptorArray : public AbstractGraphicsApi::Desc {
  public:
    DxDescriptorArray(DxDevice& dev, DxUniformsLay& vlay);
    ~DxDescriptorArray();

    void set(size_t id, AbstractGraphicsApi::Texture *tex, const Sampler2d& smp) override;
    void set(size_t id, AbstractGraphicsApi::Buffer* buf, size_t offset, size_t size, size_t align) override;

    ID3D12DescriptorHeap*      heap[3] = {};
    UINT                       heapCnt = 0;
    DSharedPtr<DxUniformsLay*> layPtr;

  private:
    DxDevice&                  dev;
  };

}}
