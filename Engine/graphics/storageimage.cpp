#include "storageimage.h"

#include <Tempest/Device>
#include <algorithm>

using namespace Tempest;

StorageImage::StorageImage(Device &, AbstractGraphicsApi::PTexture&& impl, uint32_t w, uint32_t h, TextureFormat frm)
  :impl(std::move(impl)),texW(int(w)),texH(int(h)),frm(frm) {
  }

StorageImage::StorageImage(StorageImage&& other)
  :impl(std::move(other.impl)), texW(other.texW), texH(other.texH), frm(other.frm) {
  other.texW = 0;
  other.texH = 0;
  }

StorageImage::~StorageImage(){
  }

StorageImage& StorageImage::operator=(StorageImage&& other) {
  std::swap(impl, other.impl);
  std::swap(texW, other.texW);
  std::swap(texH, other.texH);
  std::swap(frm,  other.frm);
  return *this;
  }
