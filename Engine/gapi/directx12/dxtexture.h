#pragma once

#include <Tempest/AbstractGraphicsApi>

#include <d3d12.h>
#include "gapi/directx12/comptr.h"

namespace Tempest {

namespace Detail {

class DxTexture : public AbstractGraphicsApi::Texture {
  public:
    DxTexture();
    DxTexture(ComPtr<ID3D12Resource>&& b,DXGI_FORMAT frm,UINT mips);
    DxTexture(DxTexture&& other);

    uint32_t mipCount() const override { return mips; }

    UINT     bitCount() const;

    ComPtr<ID3D12Resource> impl;
    DXGI_FORMAT            format = DXGI_FORMAT_UNKNOWN;
    UINT                   mips   = 1;
  };

}
}

