#if defined(TEMPEST_BUILD_DIRECTX12)

#include "dxrenderpass.h"

using namespace Tempest;
using namespace Tempest::Detail;

DxRenderPass::DxRenderPass(const FboMode** a, size_t acount)
  :att(acount){
  for(size_t i=0;i<acount;++i)
    att[i] = *a[i];
  }

bool DxRenderPass::isAttachPreserved(size_t i) const {
  return (att[i].mode & FboMode::PreserveIn);
  }

bool DxRenderPass::isResultPreserved(size_t i) const {
  return (att[i].mode & FboMode::PreserveOut);
  }

#endif
