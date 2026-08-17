#pragma once

#include <vector>
#include <algorithm>
#include <mutex>

#include "../utility/spinlock.h"

namespace Tempest {
namespace Detail {

template<class MemoryProvider>
class DescriptorAllocator {
  public:
    explicit DescriptorAllocator(MemoryProvider& provider) : provider(provider) {}

    struct Allocation {
      uint32_t ptr = 0xFFFFFFFF;
      };

    Allocation alloc(uint32_t num) {
      std::lock_guard<SpinLock> guard(sync);
      return rawAlloc(num);
      }

    template<class Func>
    Allocation alloc(uint32_t num, Func f) {
      std::lock_guard<SpinLock> guard(sync);
      auto ret = rawAlloc(num);
      f(ret);
      return ret;
      }

    void free(uint32_t ptr, uint32_t num) {
      if(ptr==0xFFFFFFFF || num==0)
        return;

      ptr *= provider.elementSize;
      num *= provider.elementSize;

      std::lock_guard<SpinLock> guard(sync);
      size_t i = rgn.size();
      for(; i>0;) {
        --i;
        auto& r = rgn[i];
        if(r.end<=ptr)
          break;
        }

      if(i==0 && (rgn.empty() || rgn[i].end>ptr)) {
        Range rx = {ptr, ptr+num};
        rgn.insert(rgn.begin(), rx);
        compact(0);
        return;
        }

      if(i<rgn.size()) {
        auto& r = rgn[i];
        if(r.end==ptr) {
          r.end += num;
          return compact(i);
          }
        if(i+1<rgn.size() && ptr+num==rgn[i+1].begin) {
          rgn[i+1].begin = ptr;
          return;
          }
        if(r.end<ptr) {
          Range rx = {ptr, ptr+num};
          rgn.insert(rgn.begin() + i + 1, rx);
          return compact(i + 1);
          }
        }

      // bad free
      throw std::bad_alloc();
      }

    void free(Allocation ptr, uint32_t num) {
      return free(ptr.ptr, num);
      }

    void flush() {
      provider.flush();
      }

  private:
    struct Range {
      uint32_t begin = 0;
      uint32_t end   = 0;
      };

    Allocation rawAlloc(uint32_t num) {
      const uint32_t allocSize = num*provider.elementSize;

      for(size_t i=0; i<rgn.size(); ++i) {
        auto& r = rgn[i];
        if(r.begin+allocSize > r.end)
          continue;

        uint32_t ret = r.begin;
        r.begin += allocSize;
        if(r.begin==r.end)
          rgn.erase(rgn.begin() + i);
        return Allocation{ret/provider.elementSize};
        }

      // realloc heap
      auto prevSize = provider.size();
      auto size     = std::max(prevSize, provider.reserveSize) + allocSize;
      if(size > provider.maxSize)
        throw std::bad_alloc();
#if 1
      size = std::min(std::max(nextPot(size), 4*1024u), provider.maxSize);
#else
      size = std::min(std::max(nextPot(size), 1024*1024u), provider.maxSize);
#endif
      provider.realloc(size);

      if(rgn.size()==0)
        rgn.reserve(512);

      auto allocBeg = std::max(prevSize, provider.reserveSize);
      Range& rg = rgn.emplace_back();
      rg.begin = allocBeg + allocSize;
      rg.end   = provider.size();
      return Allocation{uint32_t(allocBeg/provider.elementSize)};
      }

    void compact(size_t i) {
      if(i+1<rgn.size()) {
        auto& r0 = rgn[i+0];
        auto& r1 = rgn[i+1];
        if(r0.end==r1.begin) {
          r0.end = r1.end;
          rgn.erase(rgn.begin() + i + 1);
          }
        }
      }

    static uint32_t nextPot(uint32_t x) {
      x--;
      x |= x >> 1;
      x |= x >> 2;
      x |= x >> 4;
      x |= x >> 8;
      x |= x >> 16;
      x++;
      return x;
      }

    mutable SpinLock          sync;
    MemoryProvider&           provider;
    std::vector<Range>        rgn;
  };

}}
