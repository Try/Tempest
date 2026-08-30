#include "gapi/metal/mtshadermodulecache.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

using namespace Tempest::Detail;

namespace {

using Module = std::shared_ptr<int>;

struct ConstantHash {
  size_t operator()(const void*,size_t) const noexcept {
    return 1;
    }
  };

}

TEST(MetalShaderModuleCache,Disabled) {
  ShaderModuleCache<Module> cache(0);
  const uint32_t source = 1;
  int            compiled = 0;
  auto factory = [&]() {
    ++compiled;
    return std::make_shared<int>(compiled);
    };

  auto first  = cache.getOrCreate(&source,sizeof(source),factory);
  auto second = cache.getOrCreate(&source,sizeof(source),factory);
  EXPECT_NE(first,second);
  EXPECT_EQ(compiled,2);
  }

TEST(MetalShaderModuleCache,ReusesModule) {
  ShaderModuleCache<Module> cache(2);
  const uint32_t source = 1;
  int            compiled = 0;
  auto factory = [&]() {
    ++compiled;
    return std::make_shared<int>(compiled);
    };

  auto first  = cache.getOrCreate(&source,sizeof(source),factory);
  auto second = cache.getOrCreate(&source,sizeof(source),factory);
  EXPECT_EQ(first,second);
  EXPECT_EQ(compiled,1);
  }

TEST(MetalShaderModuleCache,EvictsLeastRecentlyUsed) {
  ShaderModuleCache<Module> cache(2);
  const uint32_t a = 1;
  const uint32_t b = 2;
  const uint32_t c = 3;
  int            compiled = 0;
  auto load = [&](const uint32_t& source) {
    return cache.getOrCreate(&source,sizeof(source),[&]() {
      ++compiled;
      return std::make_shared<int>(compiled);
      });
    };

  auto firstA = load(a);
  auto firstB = load(b);
  EXPECT_EQ(firstA,load(a));
  auto firstC = load(c);
  EXPECT_EQ(compiled,3);

  auto secondB = load(b);
  EXPECT_NE(firstB,secondB);
  EXPECT_EQ(compiled,4);

  // Eviction drops only the cache's reference. Client-held modules stay alive.
  EXPECT_EQ(*firstB,2);
  EXPECT_EQ(*firstC,3);
  }

TEST(MetalShaderModuleCache,ComparesBytesOnHashCollision) {
  ShaderModuleCache<Module,ConstantHash> cache(2);
  const uint32_t a = 1;
  const uint32_t b = 2;
  int            compiled = 0;
  auto load = [&](const uint32_t& source) {
    return cache.getOrCreate(&source,sizeof(source),[&]() {
      ++compiled;
      return std::make_shared<int>(compiled);
      });
    };

  auto firstA = load(a);
  auto firstB = load(b);
  EXPECT_NE(firstA,firstB);
  EXPECT_EQ(firstA,load(a));
  EXPECT_EQ(firstB,load(b));
  EXPECT_EQ(compiled,2);
  }

TEST(MetalShaderModuleCache,ConcurrentMissesCompileOutsideLock) {
  ShaderModuleCache<Module> cache(4);
  const uint32_t sources[] = {1,1,2,2};
  constexpr int  threadCount = 4;
  std::mutex              gateSync;
  std::condition_variable gate;
  int                     entered = 0;
  std::atomic_bool        timedOut{false};
  std::vector<Module> results(threadCount);
  std::vector<std::thread> threads;
  threads.reserve(threadCount);

  for(int i=0; i<threadCount; ++i) {
    threads.emplace_back([&,i]() {
      results[i] = cache.getOrCreate(&sources[i],sizeof(sources[i]),[&]() {
        std::unique_lock<std::mutex> lock(gateSync);
        ++entered;
        gate.notify_all();
        if(!gate.wait_for(lock,std::chrono::seconds(2),[&]() { return entered==threadCount; }))
          timedOut.store(true);
        return std::make_shared<int>(i);
        });
      });
    }
  for(auto& thread:threads)
    thread.join();

  EXPECT_FALSE(timedOut.load());
  EXPECT_EQ(entered,threadCount);
  EXPECT_EQ(results[0],results[1]);
  EXPECT_EQ(results[2],results[3]);
  EXPECT_NE(results[0],results[2]);
  }
