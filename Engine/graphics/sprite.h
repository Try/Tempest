#pragma once

#include <Tempest/TextureAtlas>
#include <cstdint>

namespace Tempest {

class Sprite final {
  public:
    Sprite();

    int  w() const { return texW; }
    int  h() const { return texH; }
    bool isEmpty() const { return alloc.owner==nullptr; }

    Size size() const { return Size(int(texW),int(texH)); }

    const Tempest::Texture2d& pageRawData(Device &dev) const;
    const Rect                pageRect() const;

    void*                     pageId() const;

  private:
    Sprite(TextureAtlas::Allocation a,uint32_t w,uint32_t h);

    TextureAtlas::Allocation alloc;
    int                      texW=0;
    int                      texH=0;

  friend class TextureAtlas;
  };

}
