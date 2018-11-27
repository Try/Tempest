#pragma once

#include <Tempest/Pixmap>
#include <Tempest/Rect>

#include <vector>
#include <mutex>
#include <atomic>

namespace Tempest {

class Device;
class Sprite;

class TextureAtlas {
  public:
    TextureAtlas(Device& device);
    TextureAtlas(const TextureAtlas&)=delete;
    virtual ~TextureAtlas();

    Sprite load(const Pixmap& pm);

  private:
    struct Page {
      Page(int w,int h):rect(1),useCount(0){
        rect[0]=Rect(0,0,w,h);
        cpu=Pixmap(uint32_t(w),uint32_t(h),Pixmap::Format::RGBA);
        }

      Page*                 next=nullptr;
      Pixmap                cpu;
      std::vector<Rect>     rect;
      std::atomic<int32_t>  useCount;
      std::mutex            sync;

      void addRef(){ useCount.fetch_add(1); }
      void decRef(){
        if(useCount.fetch_add(-1)==1)
          delete this;
        }
      };

    Device& device;
    int     defPgSize=512; //TODO
    Page    root;

    std::mutex sync;

    Sprite  tryLoad(Page& dest, const Pixmap& p);
    Sprite  emplace(Page& dest, const Pixmap& p, uint32_t x, uint32_t y);

  friend class Sprite;
  };

}
