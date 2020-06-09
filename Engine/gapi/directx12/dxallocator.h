#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <d3d12.h>

#include "dxcommandbuffer.h"

namespace Tempest {
namespace Detail {

class DxDevice;
class DxBuffer;
class DxTexture;

class DxAllocator {
  public:
    DxAllocator();

    void setDevice(DxDevice& device);

    DxBuffer  alloc(const void *mem, size_t count,  size_t size, size_t alignedSz, MemUsage usage, BufferHeap bufFlg);
    DxTexture alloc(const Pixmap &pm, uint32_t mip, DXGI_FORMAT format);
    DxTexture alloc(const uint32_t w, const uint32_t h, const uint32_t mip, TextureFormat frm);

  private:
    ID3D12Device*   device=nullptr;
  };

}
}

