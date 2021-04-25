#include "mtrenderpass.h"

#include <Tempest/RenderPass>

using namespace Tempest;
using namespace Tempest::Detail;

MtRenderPass::MtRenderPass(const FboMode **att, size_t acount)
  :mode(acount) {
  for(size_t i=0; i<acount; ++i)
    mode[i] = *att[i];
  }

bool MtRenderPass::isCompatible(const MtRenderPass &other) const {
  if(this==&other)
    return true;
  if(mode.size()!=other.mode.size())
    return false;
  for(size_t i=0; i<mode.size(); ++i)
    if(mode[i].clear!=other.mode[i].clear ||
       mode[i].mode !=other.mode[i].mode)
      return false;
  return true;
  }
