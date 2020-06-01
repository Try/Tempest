#pragma once

#include <Tempest/AbstractGraphicsApi>

#include "dxuniformslay.h"

namespace Tempest {
namespace Detail {

class VUniformsLay;

class DxDescriptorArray : public AbstractGraphicsApi::Desc {
  public:
    DxDescriptorArray(DxDevice& dev, const UniformsLayout& lay, DxUniformsLay& vlay);

    void set(size_t id, AbstractGraphicsApi::Texture *tex) override;
    void set(size_t id, AbstractGraphicsApi::Buffer* buf, size_t offset, size_t size, size_t align) override;

    ComPtr<ID3D12DescriptorHeap> heap[2];

  private:
    DxDevice& dev;
    DSharedPtr<DxUniformsLay*> layPtr;
  };

}}
