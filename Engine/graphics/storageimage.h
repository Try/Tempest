#pragma once

#include <Tempest/AbstractGraphicsApi>
#include "../utility/dptr.h"

namespace Tempest {

class StorageImage final {
  public:
    StorageImage()=default;
    StorageImage(StorageImage&&);
    ~StorageImage();
    StorageImage& operator=(StorageImage&&);

    int           w() const { return texW; }
    int           h() const { return texH; }
    Size          size() const { return Size(texW,texH); }
    bool          isEmpty() const { return texW<=0 || texH<=0; }
    TextureFormat format() const { return frm; }

  private:
    StorageImage(Tempest::Device& dev,AbstractGraphicsApi::PTexture&& impl,uint32_t w,uint32_t h,TextureFormat frm);

    Detail::DSharedPtr<AbstractGraphicsApi::Texture*> impl;
    int                                               texW=0;
    int                                               texH=0;
    TextureFormat                                     frm=Undefined;

  friend class Tempest::Device;
  friend class Tempest::DescriptorSet;
  friend class Encoder<Tempest::CommandBuffer>;

  template<class T>
  friend class Tempest::Detail::ResourcePtr;
  };

}
