#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <mutex>

#include <Metal/Metal.hpp>

namespace Tempest {
namespace Detail {

class MtDevice;

struct MtTimepoint {
  MTL::CommandBufferStatus status = MTL::CommandBufferStatusNotEnqueued;
  MTL::CommandBufferError  error  = MTL::CommandBufferErrorNone;
  std::string              errorStr;
  std::string              errorLog;

  bool                     isFinalStatus() const;
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
