#include "vtexture.h"

#include <Tempest/Pixmap>
#include "vdevice.h"

using namespace Tempest::Detail;

VTexture::VTexture(VTexture &&other) {
  std::swap(impl,  other.impl);
  std::swap(alloc, other.alloc);
  std::swap(page,  other.page);
  std::swap(device,other.device);
  }

VTexture::~VTexture() {
  if(alloc!=nullptr)
    alloc->free(*this);
  }
