#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <Tempest/Log>

#include <cstdlib>
#include <cstdint>
#include <mutex>
#include <atomic>
#include <vector>

#include "utility/spinlock.h"

namespace Tempest {

namespace Detail {

template<class Device, class CommandBuffer, class Fence>
class UploadEngine final {
  private:
    class DataStream;

  public:
    UploadEngine(Device& dev);
    ~UploadEngine();

    DataStream& get();
    void        wait();

    using Texture = AbstractGraphicsApi::Texture;
    using Buffer  = AbstractGraphicsApi::Buffer;

    using ResPtr  = Detail::DSharedPtr<AbstractGraphicsApi::Shared*>;
    using BufPtr  = Detail::DSharedPtr<Buffer*>;
    using TexPtr  = Detail::DSharedPtr<Texture*>;

    class Data final {
      public:
        Data(Device& dev)
          :sync(dev.data->allocSync), stream(dev.data->get()) {
          }
        ~Data() noexcept(false) {
          try {
            commit();
            }
          catch(...) {
            std::hash<std::thread::id> h;
            Tempest::Log::e("data queue commit failed at thread[",h(std::this_thread::get_id()),"]");
            throw;
            }
          }

        void copy(Buffer&  dest, const Buffer& src, size_t size) {
          if(commited){
            stream.begin();
            commited=false;
            }
          stream.cmdBuffer.copy(dest,0,src,0,size);
          }
        void copy(Texture& dest, uint32_t w, uint32_t h, uint32_t mip, const Buffer&  src, size_t offset) {
          if(commited){
            stream.begin();
            commited=false;
            }
          stream.cmdBuffer.copy(dest,w,h,mip,src,offset);
          }
        void copy(Buffer&  dest, uint32_t w, uint32_t h, uint32_t mip, const Texture& src, size_t offset) {
          if(commited){
            stream.begin();
            commited=false;
            }
          stream.cmdBuffer.copy(dest,w,h,mip,src,offset);
          }
        void changeLayout(Texture& dest, TextureLayout oldLayout, TextureLayout newLayout, uint32_t mipCnt) {
          if(commited){
            stream.begin();
            commited=false;
            }
          stream.cmdBuffer.changeLayout(dest,oldLayout,newLayout,mipCnt);
          }
        void generateMipmap(Texture& image, uint32_t texWidth, uint32_t texHeight, uint32_t mipLevels) {
          if(commited){
            stream.begin();
            commited=false;
            }
          stream.cmdBuffer.generateMipmap(image,texWidth,texHeight,mipLevels);
          }

        void hold(BufPtr &b) {
          stream.hold.emplace_back(ResPtr(b.handler));
          }

        void hold(TexPtr &b) {
          if(commited){
            stream.begin();
            commited=false;
            }
          stream.hold.emplace_back(ResPtr(b.handler));
          }

        void commit() {
          if(commited)
            return;
          commited=true;
          stream.end();
          }

      private:
        std::lock_guard<std::mutex> sync;
        DataStream&                 stream;
        bool                        commited=true;
      };

  private:
    enum  DataState : uint8_t {
      StIdle      = 0,
      StRecording = 1,
      StWait      = 2,
      };

    class DataStream {
      public:
        DataStream(Device &owner):owner(owner), cmdBuffer(owner), fence(owner) {
          hold.reserve(32);
          }
        ~DataStream() { wait(); }

        void begin() {
          cmdBuffer.begin();
          }

        void end() {
          cmdBuffer.end();
          owner.submit(cmdBuffer,fence);
          state.store(StWait);
          }

        void wait() {
          while(true) {
            auto s = state.load();
            if(s==StIdle)
              return;
            if(s==StWait)  {
              fence.wait();
              cmdBuffer.reset();
              hold.clear();
              state.store(StIdle);
              return;
              }
            }
          }

        Device&                owner;
        CommandBuffer          cmdBuffer;
        std::vector<ResPtr>    hold;

        Fence                  fence;
        std::atomic<DataState> state{StIdle};
      };

    std::mutex                  allocSync;

    static constexpr size_t     size=2;
    SpinLock                    sync[size];
    std::unique_ptr<DataStream> streams[size];
    std::atomic_int             at{0};
  };

template<class Device, class CommandBuffer, class Fence>
UploadEngine<Device,CommandBuffer,Fence>::UploadEngine(Device& dev) {
  for(auto& i:streams)
    i.reset(new DataStream(dev));
  }

template<class Device, class CommandBuffer, class Fence>
UploadEngine<Device,CommandBuffer,Fence>::~UploadEngine() {
  wait();
  }

template<class Device, class CommandBuffer, class Fence>
auto UploadEngine<Device,CommandBuffer,Fence>::get() -> DataStream& {
  auto id = at.fetch_add(1)%2;

  std::lock_guard<SpinLock> guard(sync[id]);
  auto& st = *streams[id];
  st.wait();
  st.state.store(StRecording);
  return st;
  }

template<class Device, class CommandBuffer, class Fence>
void UploadEngine<Device,CommandBuffer,Fence>::wait() {
  for(size_t i=0;i<size;++i) {
    std::lock_guard<SpinLock> guard(sync[i]);
    streams[i]->wait();
    }
  }

}}


