#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <mutex>
#include <atomic>

#include <Metal/Metal.hpp>

namespace Tempest {
namespace Detail {

class MtDevice;

struct MtTimepoint : public AbstractGraphicsApi::Fence {
  MtTimepoint(MtDevice* device):device(device) {}

  MtDevice*                device = nullptr;
  std::atomic<MTL::CommandBufferStatus> status;
  //MTL::CommandBufferStatus status = MTL::CommandBufferStatusNotEnqueued;
  MTL::CommandBufferError  error  = MTL::CommandBufferErrorNone;
  std::string              errorStr;
  std::string              errorLog;

  void                     wait() override;
  bool                     wait(uint64_t time) override;
  void                     propogateError(const MtTimepoint& t);

  bool                     isFinalStatus() const;
  void                     clear();
  };

class MtSync : public AbstractGraphicsApi::Fence {
  public:
    MtSync();
    ~MtSync();

    MtDevice*                  device = nullptr;
    std::weak_ptr<MtTimepoint> timepoint;

    void wait() override;
    bool wait(uint64_t time) override;

  private:
    void propogateError(const MtTimepoint& t);
  };

}
}
