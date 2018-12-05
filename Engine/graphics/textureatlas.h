#pragma once

#include <Tempest/Pixmap>
#include <Tempest/Rect>
#include "../gapi/rectalllocator.h"

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
    Device& device;
    struct MemoryProvider {
      using DeviceMemory=Pixmap*;

      DeviceMemory alloc(uint32_t w,uint32_t h){
        return new Pixmap(w,h,Pixmap::Format::RGBA);
        }

      void free(DeviceMemory m){
        delete m;
        }
      };

    using Allocation = typename Tempest::RectAlllocator<MemoryProvider>::Allocation;

    MemoryProvider                          provider;
    Tempest::RectAlllocator<MemoryProvider> alloc;
    void emplace(Allocation& dest, const Pixmap& p, uint32_t x, uint32_t y);

  friend class Sprite;
  };

}
