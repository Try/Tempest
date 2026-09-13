#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <Tempest/Except>
#include <Tempest/Log>

#include <cstdlib>
#include <cstdint>
#include <mutex>
#include <vector>

#include "utility/spinlock.h"

namespace Tempest {

namespace Detail {

template<class CmdBuffer>
class TransferCmd : public CmdBuffer {
  public:
    using BufPtr  = Detail::DSharedPtr<AbstractGraphicsApi::Buffer*>;
    using CBufPtr = Detail::DSharedPtr<const AbstractGraphicsApi::Buffer*>;
    using TexPtr  = Detail::DSharedPtr<AbstractGraphicsApi::Texture*>;
    using AsPtr   = Detail::DSharedPtr<AbstractGraphicsApi::AccelerationStructure*>;
    using ResPtr  = Detail::DSharedPtr<const AbstractGraphicsApi::Shared*>;
    using Fence   = std::shared_ptr<AbstractGraphicsApi::Fence>;

    template<class Device>
    TransferCmd(Device& dev):CmdBuffer(dev) {
      holdRes.reserve(4);
      }

    void hold(BufPtr &b) {
      holdRes.emplace_back(ResPtr(b.handler));
      }

    void hold(CBufPtr &b) {
      holdRes.emplace_back(ResPtr(b.handler));
      }

    void hold(TexPtr &b) {
      holdRes.emplace_back(ResPtr(b.handler));
      }

    void hold(AsPtr &b) {
      holdRes.emplace_back(ResPtr(b.handler));
      }

    bool wait(uint64_t t) {
      if(fence!=nullptr && !fence->wait(t))
        return false;
      holdRes.clear();
      return true;
      }

    void wait() {
      if(fence!=nullptr)
        fence->wait();
      holdRes.clear();
      }

    void reset() {
      holdRes.clear();
      CmdBuffer::reset();
      }

    void setFence(Fence f) {
      fence = f;
      fence->setPayload(std::move(holdRes));
      }

  private:
    std::vector<ResPtr> holdRes;
    Fence               fence;
  };

template<class Device, class CommandBuffer, class Buffer>
class UploadEngine final {
  public:
    UploadEngine(Device& dev):device(dev){}
    ~UploadEngine() {
      wait();
      }

    using Commands = TransferCmd<CommandBuffer>;

    std::unique_ptr<Commands> get();
    void                      submit(std::unique_ptr<Commands>&& cmd);
    void                      submitAndWait(std::unique_ptr<Commands>&& cmd);

    Buffer                    allocStagingMemory(const void* data, size_t count, size_t size, size_t alignedSz, MemUsage usage, BufferHeap heap);
    Buffer                    allocStagingMemory(const void* data, size_t size, MemUsage usage, BufferHeap heap);

  private:
    void                      wait();

    Device&                   device;
    SpinLock                  sync;
    std::vector<std::unique_ptr<Commands>> cmd;
  };

template<class Device, class CommandBuffer, class Buffer>
auto UploadEngine<Device,CommandBuffer,Buffer>::get() -> std::unique_ptr<Commands> {
  std::lock_guard<SpinLock> guard(sync);

  std::unique_ptr<Commands> ret;
  size_t                    i = 0;
  constexpr size_t          minCmd = 4;
  for(i = 0; i<cmd.size(); ++i) {
    if(cmd[i]->wait(0)) {
      std::swap(cmd[i],cmd.back());
      ret = std::move(cmd.back());
      cmd.pop_back();
      break;
      }
    }

  if(ret==nullptr)
    return std::unique_ptr<Commands>{new Commands(device)};

  while(i < cmd.size()) {
    if(cmd.size()<=minCmd)
      return ret;

    if(cmd[i]->wait(0)) {
      std::swap(cmd[i],cmd.back());
      cmd.pop_back();
      } else {
      ++i;
      }
    }

  return ret;
  }

template<class Device, class CommandBuffer, class Buffer>
void UploadEngine<Device,CommandBuffer,Buffer>::wait() {
  std::lock_guard<SpinLock> guard(sync);
  for(auto& i:cmd)
    i->wait();
  }

template<class Device, class CommandBuffer, class Buffer>
void UploadEngine<Device,CommandBuffer,Buffer>::submit(std::unique_ptr<Commands>&& cmd) {
  cmd->setFence(device.submit(*cmd));

  std::lock_guard<SpinLock> guard(sync);
  this->cmd.push_back(std::move(cmd));
  }

template<class Device, class CommandBuffer, class Buffer>
void UploadEngine<Device,CommandBuffer,Buffer>::submitAndWait(std::unique_ptr<Commands>&& cmd) {
  auto ptr = device.submit(*cmd);
  if(ptr!=nullptr)
    ptr->wait();
  cmd->reset();

  std::lock_guard<SpinLock> guard(sync);
  this->cmd.push_back(std::move(cmd));
  }

template<class Device, class CommandBuffer, class Buffer>
Buffer UploadEngine<Device,CommandBuffer,Buffer>::allocStagingMemory(const void* data, size_t count, size_t size, size_t alignedSz, MemUsage usage, BufferHeap heap) {
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

template<class Device, class CommandBuffer, class Buffer>
Buffer UploadEngine<Device,CommandBuffer,Buffer>::allocStagingMemory(const void* data, size_t size, MemUsage usage, BufferHeap heap) {
  try {
    return device.allocator.alloc(data,size,usage,heap);
    }
  catch(std::system_error& err) {
    if(err.code()!=Tempest::GraphicsErrc::OutOfVideoMemory)
      throw;
    // wait for other staging resources to be released
    wait();
    return device.allocator.alloc(data,size,usage,heap);
    }
  }
}}


