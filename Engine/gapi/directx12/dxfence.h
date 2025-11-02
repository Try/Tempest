#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <d3d12.h>
#include "gapi/directx12/comptr.h"
#include "gapi/directx12/dxevent.h"

namespace Tempest {
namespace Detail {

class DxDevice;

struct DxTimepoint {
  DxTimepoint(DxEvent f):event(std::move(f)) {}

  DxEvent  event;
  uint64_t signalValue = 0;
  };

class DxFence : public AbstractGraphicsApi::Fence {
  public:
    DxFence();
    ~DxFence() override;

    void wait() override;
    bool wait(uint64_t timeout) override;

    DxDevice*                  device = nullptr;
    std::weak_ptr<DxTimepoint> timepoint;
  };

}}
