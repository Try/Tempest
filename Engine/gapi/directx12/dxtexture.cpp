#if defined(TEMPEST_BUILD_DIRECTX12)

#include "dxtexture.h"

using namespace Tempest;
using namespace Tempest::Detail;

DxTexture::DxTexture() {
  }

DxTexture::DxTexture(ComPtr<ID3D12Resource>&& b)
  :impl(std::move(b)) {
  }

DxTexture::DxTexture(DxTexture&& other)
  :impl(std::move(other.impl)) {
  }

void DxTexture::setSampler(const Sampler2d& ) {
  }

#endif
