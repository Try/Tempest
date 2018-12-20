#include "texture2d.h"

#include <Tempest/Device>
#include <algorithm>

using namespace Tempest;

Texture2d::Texture2d(Device&, AbstractGraphicsApi::Texture *impl, uint32_t w, uint32_t h)
  :impl(impl),texW(int(w)),texH(int(h)) {
  }

Texture2d::~Texture2d(){
  }

