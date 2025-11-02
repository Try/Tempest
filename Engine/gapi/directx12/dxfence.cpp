#if defined(TEMPEST_BUILD_DIRECTX12)

#include "dxfence.h"

#include "dxdevice.h"

using namespace Tempest;
using namespace Tempest::Detail;

void DxFence::wait() {
  dxAssert(device->waitFence(*this, std::numeric_limits<uint64_t>::max()));
  }

bool DxFence::wait(uint64_t timeout) {
  HRESULT res = device->waitFence(*this, timeout);
  if(res==WAIT_TIMEOUT)
    return false;
  dxAssert(res);
  return true;
  }

#endif

