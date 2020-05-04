#pragma once

#include <Tempest/AbstractGraphicsApi>

#include <d3d12.h>
#include "gapi/directx12/comptr.h"

namespace Tempest {

namespace Detail {

class DxTexture : public AbstractGraphicsApi::Texture {
  public:
    DxTexture();
    DxTexture(ComPtr<ID3D12Resource>&& b);
    DxTexture(DxTexture&& other);

    void setSampler(const Sampler2d& s);

    ComPtr<ID3D12Resource> impl;
  };

}
}

