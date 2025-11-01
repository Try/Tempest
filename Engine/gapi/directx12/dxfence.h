#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <d3d12.h>
#include "comptr.h"

namespace Tempest {
namespace Detail {

class DxDevice;

class DxFence : public AbstractGraphicsApi::Fence {
  public:
    enum State : uint64_t {
      Waiting = 0,
      Ready   = 1
      };
    DxFence(DxDevice& dev);
    ~DxFence() override;

    void wait() override;
    bool wait(uint64_t timeout) override;
    void reset();

    bool waitValue(UINT64 val, DWORD timeout = INFINITE);

    void signal(ID3D12CommandQueue& queue);
    void signal(ID3D12CommandQueue& queue,UINT64 val);

    ComPtr<ID3D12Fence> impl;
    DxDevice&           device;
    HANDLE              event=nullptr;
  };

}}
