#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <Tempest/Except>
#include <Tempest/Log>

#include <cstdlib>
#include <cstdint>
#include <mutex>
#include <atomic>
#include <vector>

#include "utility/spinlock.h"

namespace Tempest {

namespace Detail {

template<class CmdBuffer, class Fence>
class TransferCmd : public CmdBuffer {
  public:
    using BufPtr = Detail::DSharedPtr<AbstractGraphicsApi::Buffer*>;
    using TexPtr = Detail::DSharedPtr<AbstractGraphicsApi::Texture*>;
    using ResPtr = Detail::DSharedPtr<AbstractGraphicsApi::Shared*>;

    template<class Device>
    TransferCmd(Device& dev):CmdBuffer(dev), fence(dev) {
      holdRes.reserve(4);
      }

    void hold(BufPtr &b) {
      holdRes.emplace_back(ResPtr(b.handler));
      }

    void hold(TexPtr &b) {
      holdRes.emplace_back(ResPtr(b.handler));
      }

    bool wait(uint64_t t) {
      if(!fence.wait(t))
        return false;
      holdRes.clear();
      return true;
      }
    void wait()           {
      fence.wait();
      holdRes.clear();
      }

    bool waitFor(AbstractGraphicsApi::Shared* s) {
      for(auto& i:holdRes)
        if(i.handler==s) {
          wait();
          return true;
          }
      return false;
      }

    void reset() {
      holdRes.clear();
      CmdBuffer::reset();
      }

    Fence               fence;

  private:
    std::vector<ResPtr> holdRes;
  };

template<class Device, class CommandBuffer, class Fence, class Buffer>
class UploadEngine final {
  public:
    UploadEngine(Device& dev):device(dev){}
    ~UploadEngine() {
      wait();
      }

    using Commands = TransferCmd<CommandBuffer,Fence>;

    std::unique_ptr<Commands> get();
    void                      submit(std::unique_ptr<Commands>&& cmd);
    void                      submitAndWait(std::unique_ptr<Commands>&& cmd);
    void                      wait();
    void                      waitFor(AbstractGraphicsApi::Shared* s);

    Buffer                    allocStagingMemory(const void* data, size_t count, size_t size, size_t alignedSz, MemUsage usage, BufferHeap heap);

  private:
    Device&                   device;

    SpinLock                  sync;
    std::vector<std::unique_ptr<Commands>> cmd;
    bool                      hasWaits {false};
  };

template<class Device, class CommandBuffer, class Fence, class Buffer>
auto UploadEngine<Device,CommandBuffer,Fence,Buffer>::get() -> std::unique_ptr<Commands> {
  {
  std::lock_guard<SpinLock> guard(sync);
  if(!hasWaits && cmd.size()>0) {
    auto ret = std::move(cmd.back());
    cmd.pop_back();
    if(cmd.size()>4)
      cmd.resize(4);
    return ret;
    }
  for(size_t i=0; i<cmd.size(); ++i) {
    if(cmd[i]->wait(0)) {
      std::swap(cmd[i],cmd.back());
      auto ret = std::move(cmd.back());
      cmd.pop_back();
      return ret;
      }
    }
  }
  return std::unique_ptr<Commands>{new Commands(device)};
  }

template<class Device, class CommandBuffer, class Fence, class Buffer>
void UploadEngine<Device,CommandBuffer,Fence,Buffer>::wait() {
  std::lock_guard<SpinLock> guard(sync);
  if(!hasWaits)
    return;
  for(auto& i:cmd)
    i->wait();
  hasWaits = false;
  }

template<class Device, class CommandBuffer, class Fence, class Buffer>
void UploadEngine<Device,CommandBuffer,Fence,Buffer>::waitFor(AbstractGraphicsApi::Shared* s) {
  std::lock_guard<SpinLock> guard(sync);
  if(!hasWaits)
    return;
  bool waitAll = true;
  for(auto& i:cmd)
    waitAll &= i->waitFor(s);
  hasWaits = !waitAll;
  }

template<class Device, class CommandBuffer, class Fence, class Buffer>
void UploadEngine<Device,CommandBuffer,Fence,Buffer>::submit(std::unique_ptr<Commands>&& cmd) {
  device.submit(*cmd,&cmd->fence);

  std::lock_guard<SpinLock> guard(sync);
  this->cmd.push_back(std::move(cmd));
  hasWaits = true;
  }

template<class Device, class CommandBuffer, class Fence, class Buffer>
void UploadEngine<Device,CommandBuffer,Fence,Buffer>::submitAndWait(std::unique_ptr<Commands>&& cmd) {
  device.submit(*cmd,&cmd->fence);
  cmd->fence.wait();
  cmd->reset();

  std::lock_guard<SpinLock> guard(sync);
  this->cmd.push_back(std::move(cmd));
  }

template<class Device, class CommandBuffer, class Fence, class Buffer>
Buffer UploadEngine<Device, CommandBuffer, Fence,Buffer>::allocStagingMemory(const void* data, size_t count, size_t size, size_t alignedSz, MemUsage usage, BufferHeap heap) {
  try {
    return device.allocator.alloc(data,count,size,alignedSz,usage,heap);
    }
  catch(std::system_error& err) {
    if(err.code()!=Tempest::GraphicsErrc::OutOfVideoMemory)
      throw;
    // wait for other staging resources to be released
    wait();
    return device.allocator.alloc(data,count,size,alignedSz,usage,heap);
    }
  }

}}


