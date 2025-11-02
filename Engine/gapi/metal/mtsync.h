#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <mutex>
#include <atomic>

#include <Metal/Metal.hpp>

namespace Tempest {
namespace Detail {

class MtDevice;

struct MtFence : public AbstractGraphicsApi::Fence {
  MtFence(MtDevice* device):device(device) {}

  MtDevice*                device = nullptr;
  std::atomic<MTL::CommandBufferStatus> status;
  //MTL::CommandBufferStatus status = MTL::CommandBufferStatusNotEnqueued;
  MTL::CommandBufferError  error  = MTL::CommandBufferErrorNone;
  std::string              errorStr;
  std::string              errorLog;

  void                     wait() override;
  bool                     wait(uint64_t time) override;
  void                     propogateError();

  bool                     isFinalStatus() const;
  void                     clear();
  };
}
}
