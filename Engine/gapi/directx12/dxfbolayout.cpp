#if defined(TEMPEST_BUILD_DIRECTX12)

#include "dxfbolayout.h"

using namespace Tempest;
using namespace Tempest::Detail;

bool DxFboLayout::equals(const DxFboLayout& oth) const {
  if(NumRenderTargets!=oth.NumRenderTargets)
    return false;
  for(UINT i=0;i <NumRenderTargets; ++i)
    if(RTVFormats[i]!=oth.RTVFormats[i])
      return false;
  return DSVFormat==oth.DSVFormat;
  }

#endif
