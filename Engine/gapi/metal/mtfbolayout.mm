#include "mtfbolayout.h"

#include <Tempest/Except>

#include "mtdevice.h"

using namespace Tempest;
using namespace Tempest::Detail;

MtFboLayout::MtFboLayout(MtSwapchain** sw, TextureFormat* att, size_t attCount) {
  for(uint8_t i=0; i<attCount; ++i) {
    MTLPixelFormat frm = MTLPixelFormatInvalid;
    //if(att[i]==TextureFormat::Undefined)
    //  frm = sw[i]->format(); else
      frm = Detail::nativeFormat(att[i]);

    if(Tempest::isDepthFormat(att[i])) {
      if(depthFormat!=MTLPixelFormatInvalid)
        throw IncompleteFboException();
      depthFormat = frm;
      } else {
      if(numColors==256)
        throw IncompleteFboException();
      colorFormat[numColors] = frm;
      ++numColors;
      }
    }
  }

bool MtFboLayout::equals(const AbstractGraphicsApi::FboLayout &oth) const {
  auto& other = reinterpret_cast<const MtFboLayout&>(oth);
  if(depthFormat!=other.depthFormat || numColors!=other.numColors)
    return false;
  for(size_t i=0; i<numColors; ++i)
    if(colorFormat[i]!=other.colorFormat[i])
      return false;
  return true;
  }
