#if defined(TEMPEST_BUILD_DIRECTX12)

#include "dxrenderpass.h"

using namespace Tempest;
using namespace Tempest::Detail;

DxRenderPass::DxRenderPass(const FboMode** a, size_t acount)
  :att(acount){
  for(size_t i=0;i<acount;++i)
    att[i] = *a[i];
  }

#endif
