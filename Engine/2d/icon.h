#pragma once

#include <Tempest/Pixmap>
#include <Tempest/Sprite>

#include <vector>

namespace Tempest {

class Icon {
  public:
    Icon();
    Icon(const Pixmap& pm, TextureAtlas& h);

    enum State {
      ST_Normal  =0,
      ST_Disabled=1,
      ST_Count   =2
      };

    const Sprite& sprite(int w,int h,State st) const;
    void          set(State st,const Sprite& s);

  private:
    struct SzArray {
      Sprite              emplace;
      std::vector<Sprite> data;

      const Sprite& sprite(int w,int h) const;
      void          set   (const Sprite& s);
      };
    SzArray val[ST_Count];
  };

}
