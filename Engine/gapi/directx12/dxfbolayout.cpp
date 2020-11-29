#if defined(TEMPEST_BUILD_DIRECTX12)

#include "dxfbolayout.h"
#include "dxdevice.h"
#include "dxswapchain.h"

using namespace Tempest;
using namespace Tempest::Detail;

DxFboLayout::DxFboLayout(DxSwapchain** sw, TextureFormat* att, size_t attCount) {
  for(uint8_t i=0;i<attCount;++i) {
    DXGI_FORMAT frm = DXGI_FORMAT_UNKNOWN;
    if(att[i]==TextureFormat::Undefined)
      frm = sw[i]->format(); else
      frm = Detail::nativeFormat(att[i]);

    if(Tempest::isDepthFormat(att[i])) {
      if(DSVFormat!=DXGI_FORMAT_UNKNOWN)
        throw IncompleteFboException();
      DSVFormat = frm;
      } else {
      if(NumRenderTargets==8)
        throw IncompleteFboException();
      RTVFormats[NumRenderTargets] = frm;
      ++NumRenderTargets;
      }
    }
  }

bool DxFboLayout::equals(const Tempest::AbstractGraphicsApi::FboLayout& l) const {
  auto& oth = reinterpret_cast<const DxFboLayout&>(l);
  if(NumRenderTargets!=oth.NumRenderTargets)
    return false;
  for(UINT i=0;i <NumRenderTargets; ++i)
    if(RTVFormats[i]!=oth.RTVFormats[i])
      return false;
  return DSVFormat==oth.DSVFormat;
  }

#endif

Tempest::Detail::DxFboLayout& Tempest::Detail::DxFboLayout::operator =(const Tempest::Detail::DxFboLayout& other) {
  NumRenderTargets = other.NumRenderTargets;
  std::memcpy(RTVFormats,other.RTVFormats,sizeof(RTVFormats));
  DSVFormat = other.DSVFormat;
  return *this;
  }

Tempest::Detail::DxFboLayout::DxFboLayout(const Tempest::Detail::DxFboLayout& other) {
  *this = other;
  }
