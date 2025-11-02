#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <d3d12.h>
#include "gapi/directx12/dxevent.h"

namespace Tempest {
namespace Detail {

class DxDevice;

struct DxFence : public AbstractGraphicsApi::Fence {
  DxFence(DxDevice* device, DxEvent f):device(device), event(std::move(f)) {}

  void wait() override;
  bool wait(uint64_t timeout) override;

  DxDevice* device = nullptr;
  DxEvent   event;
  uint64_t  signalValue = 0;
  };

}}
