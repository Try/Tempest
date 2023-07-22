#pragma once

#include <Tempest/Texture2d>
#include <Tempest/Pixmap>
#include <Tempest/Rect>
#include "../gapi/rectallocator.h"

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
    Sprite load(const void* data,uint32_t w,uint32_t h,Pixmap::Format format);

  private:
    struct Memory {
      Memory()=default;
      Memory(uint32_t w,uint32_t h):cpu(w,h,Pixmap::Format::RGBA8){}
      Memory(Memory&&)=default;

      Memory& operator=(Memory&&)=default;

      Pixmap            cpu;
      mutable Texture2d gpu;
      mutable bool      changed=true;
      };

    struct MemoryProvider {
      using DeviceMemory=Memory;

      DeviceMemory alloc(uint32_t w,uint32_t h){
        DeviceMemory ret(w,h);
        return ret;
        }

      void free(DeviceMemory& m){
        // nop
        m=DeviceMemory();
        }
      };

    using Allocation = typename Tempest::RectAllocator<MemoryProvider>::Allocation;

    void emplace(Allocation& dest, const void *img,
                 uint32_t w, uint32_t h, Pixmap::Format frm,
                 uint32_t x, uint32_t y);

    Device&                                 device;
    MemoryProvider                          provider;
    Tempest::RectAllocator<MemoryProvider> alloc;

  friend class Sprite;
  };

}
