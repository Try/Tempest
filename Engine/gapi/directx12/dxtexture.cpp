#if defined(TEMPEST_BUILD_DIRECTX12)

#include "dxtexture.h"

using namespace Tempest;
using namespace Tempest::Detail;

DxTexture::DxTexture() {
  }

DxTexture::DxTexture(ComPtr<ID3D12Resource>&& b, DXGI_FORMAT frm, UINT mips)
  :impl(std::move(b)), format(frm), mips(mips) {
  }

DxTexture::DxTexture(DxTexture&& other)
  :impl(std::move(other.impl)), format(other.format), mips(other.mips) {
  }

void DxTexture::setSampler(const Sampler2d& ) {
  }

#endif
