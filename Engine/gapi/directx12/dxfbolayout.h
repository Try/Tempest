#pragma once

#include <Tempest/AbstractGraphicsApi>

#include <d3d12.h>

namespace Tempest {
namespace Detail {

class DxSwapchain;

class DxFboLayout : public AbstractGraphicsApi::FboLayout {
  public:
    DxFboLayout() = default;
    DxFboLayout(const DxFboLayout& other);
    DxFboLayout(Detail::DxSwapchain** sw, TextureFormat* att, size_t attCount);

    DxFboLayout& operator = (const DxFboLayout& other);

    bool equals(const FboLayout&) const override;

    UINT        NumRenderTargets = 0;
    DXGI_FORMAT RTVFormats[ 8 ] = {};
    DXGI_FORMAT DSVFormat = DXGI_FORMAT_UNKNOWN;
  };

}
}

