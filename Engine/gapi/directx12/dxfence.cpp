#if defined(TEMPEST_BUILD_DIRECTX12)

#include "dxfence.h"

#include "dxdevice.h"
#include "guid.h"

using namespace Tempest;
using namespace Tempest::Detail;

DxFence::DxFence(DxDevice& dev) : device(dev) {
  dxAssert(dev.device->CreateFence(Ready, D3D12_FENCE_FLAG_NONE,
                                   uuid<ID3D12Fence>(),
                                   reinterpret_cast<void**>(&impl)));
  event = CreateEvent(nullptr, TRUE, TRUE, nullptr);
  if(event == nullptr)
    throw std::bad_alloc();
  }

DxFence::~DxFence() {
  if(event!=nullptr)
    CloseHandle(event);
  }

void DxFence::wait() {
  waitValue(Ready);
  }

bool DxFence::wait(uint64_t timeout) {
  if(timeout>INFINITE)
    timeout = INFINITE;
  return waitValue(Ready,DWORD(timeout));
  }

bool DxFence::waitValue(UINT64 val, DWORD timeout) {
  UINT64 v = impl->GetCompletedValue();
  if(val==v)
    return true;
  if(timeout==0)
    return false;
  dxAssert(impl->SetEventOnCompletion(val,event));
  DWORD ret = WaitForSingleObjectEx(event, timeout, FALSE);
  if(ret==WAIT_OBJECT_0)
    return true;
  if(ret==WAIT_TIMEOUT)
    return false;
  dxAssert(GetLastError(), device);
  return false;
  }

void DxFence::reset() {
  ResetEvent(event);
  dxAssert(impl->Signal(Waiting));
  }

void DxFence::signal(ID3D12CommandQueue& queue) {
  signal(queue,DxFence::Ready);
  }

void DxFence::signal(ID3D12CommandQueue& queue, UINT64 val) {
  dxAssert(queue.Signal(impl.get(),val), device);
  }

#endif
