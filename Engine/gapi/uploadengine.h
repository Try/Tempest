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

    bool wait(uint64_t t) { return fence.wait(t); }
    void wait()           {
      fence.wait();
      holdRes.clear();
      }

    void reset() {
      holdRes.clear();
      CmdBuffer::reset();
      }

    Fence               fence;

  private:
    std::vector<ResPtr> holdRes;
  };

template<class Device, class CommandBuffer, class Fence>
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

  private:
    Device&                  device;

    SpinLock                 sync;
    std::vector<std::unique_ptr<Commands>> cmd;
    bool                     hasWaits {false};
  };

template<class Device, class CommandBuffer, class Fence>
auto UploadEngine<Device,CommandBuffer,Fence>::get() -> std::unique_ptr<Commands> {
  {
  std::lock_guard<SpinLock> guard(sync);
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

template<class Device, class CommandBuffer, class Fence>
void UploadEngine<Device,CommandBuffer,Fence>::wait() {
  std::lock_guard<SpinLock> guard(sync);
  if(!hasWaits)
    return;
  for(auto& i:cmd)
    i->wait();
  hasWaits = false;
  }

template<class Device, class CommandBuffer, class Fence>
void UploadEngine<Device,CommandBuffer,Fence>::submit(std::unique_ptr<Commands>&& cmd) {
  device.submit(*cmd,cmd->fence);

  std::lock_guard<SpinLock> guard(sync);
  this->cmd.push_back(std::move(cmd));
  hasWaits = true;
  }

template<class Device, class CommandBuffer, class Fence>
void UploadEngine<Device,CommandBuffer,Fence>::submitAndWait(std::unique_ptr<Commands>&& cmd) {
  device.submit(*cmd,cmd->fence);
  cmd->fence.wait();
  cmd->reset();

  std::lock_guard<SpinLock> guard(sync);
  this->cmd.push_back(std::move(cmd));
  }

}}


