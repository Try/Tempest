#include "texture2d.h"

#include <Tempest/Device>
#include <algorithm>

using namespace Tempest;

Texture2d::Texture2d(Device &, AbstractGraphicsApi::PTexture&& impl, uint32_t w, uint32_t h, TextureFormat frm)
  :impl(std::move(impl)),texW(int(w)),texH(int(h)),frm(frm) {
  }

Texture2d::~Texture2d(){
  }
