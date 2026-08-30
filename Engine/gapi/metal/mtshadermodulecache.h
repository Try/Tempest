#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <list>
#include <mutex>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Tempest {
namespace Detail {

struct ShaderModuleHash {
  size_t operator()(const void* source, size_t size) const noexcept {
    // FNV-1a is used only to select a bucket. Cache hits always compare the
    // complete shader module, so hash collisions cannot alias two modules.
    constexpr size_t offset = sizeof(size_t)==8 ? size_t(14695981039346656037ull) : size_t(2166136261u);
    constexpr size_t prime  = sizeof(size_t)==8 ? size_t(1099511628211ull)        : size_t(16777619u);

    auto*  bytes = static_cast<const uint8_t*>(source);
    size_t hash  = offset;
    for(size_t i=0; i<size; ++i) {
      hash ^= bytes[i];
      hash *= prime;
      }
    return hash;
    }
  };

template<class Value, class Hash = ShaderModuleHash>
class ShaderModuleCache final {
  public:
    explicit ShaderModuleCache(size_t capacity, Hash hash = Hash{})
      : capacity(capacity), hash(std::move(hash)) {
      }

    ShaderModuleCache(const ShaderModuleCache&) = delete;
    ShaderModuleCache& operator=(const ShaderModuleCache&) = delete;

    template<class Factory>
    Value getOrCreate(const void* source, size_t size, Factory&& factory) {
      if(capacity==0)
        return std::forward<Factory>(factory)();

      const size_t hashValue = hash(source,size);
      {
        std::lock_guard<std::mutex> guard(sync);
        auto at = find(hashValue,source,size);
        if(at!=entries.end()) {
          entries.splice(entries.begin(),entries,at);
          return entries.front().value;
          }
        }

      std::vector<uint8_t> key(size);
      if(size>0)
        std::memcpy(key.data(),source,size);

      // Shader translation and Metal compilation are intentionally outside
      // the cache lock. Concurrent misses may compile the same module; the
      // second lookup below selects a single shared cache entry.
      Value candidate = std::forward<Factory>(factory)();
      Value evicted;
      {
        std::lock_guard<std::mutex> guard(sync);
        auto at = find(hashValue,key.data(),key.size());
        if(at!=entries.end()) {
          entries.splice(entries.begin(),entries,at);
          return entries.front().value;
          }

        entries.push_front(Entry{hashValue,std::move(key),candidate});
        try {
          buckets.emplace(hashValue,entries.begin());
          }
        catch(...) {
          entries.pop_front();
          throw;
          }

        if(entries.size()>capacity) {
          auto last = std::prev(entries.end());
          eraseBucket(last);
          evicted = std::move(last->value);
          entries.erase(last);
          }
        }
      // Keep destruction of an evicted module outside the cache lock.
      return candidate;
      }

  private:
    struct Entry {
      size_t               hash = 0;
      std::vector<uint8_t> source;
      Value                value;
      };

    using EntryList = std::list<Entry>;
    using Iterator  = typename EntryList::iterator;

    Iterator find(size_t hashValue, const void* source, size_t size) {
      auto range = buckets.equal_range(hashValue);
      for(auto i=range.first; i!=range.second; ++i) {
        auto at = i->second;
        if(at->source.size()!=size)
          continue;
        if(size==0 || std::memcmp(at->source.data(),source,size)==0)
          return at;
        }
      return entries.end();
      }

    void eraseBucket(Iterator entry) {
      auto range = buckets.equal_range(entry->hash);
      for(auto i=range.first; i!=range.second; ++i) {
        if(i->second==entry) {
          buckets.erase(i);
          return;
          }
        }
      }

    const size_t capacity;
    Hash         hash;

    std::mutex sync;
    EntryList entries;
    std::unordered_multimap<size_t,Iterator> buckets;
  };

}
}
