#include "mtfbolayout.h"

#include <Tempest/Except>

#include "mtdevice.h"
#include "mtswapchain.h"

using namespace Tempest;
using namespace Tempest::Detail;

bool MtFboLayout::equals(const MtFboLayout& oth) const {
  auto& other = reinterpret_cast<const MtFboLayout&>(oth);
  if(depthFormat!=other.depthFormat || numColors!=other.numColors)
    return false;
  for(size_t i=0; i<numColors; ++i)
    if(colorFormat[i]!=other.colorFormat[i])
      return false;
  return true;
  }
