#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <d3d12.h>
#include "gapi/directx12/dxevent.h"

namespace Tempest {
namespace Detail {

class DxDevice;

struct DxFence : public AbstractGraphicsApi::Fence {
  using ResPtr  = Detail::DSharedPtr<const AbstractGraphicsApi::Shared*>;

  DxFence(DxDevice* device, DxEvent f):device(device), event(std::move(f)) {}

  void wait() override;
  bool wait(uint64_t timeout) override;

  void setPayload(std::vector<ResPtr>&&) override;
  void clearPayload();

  DxDevice*           device = nullptr;
  DxEvent             event;
  uint64_t            signalValue = 0;
  std::vector<ResPtr> holdRes;
  };

}}
