#include "mtdevice.h"

#include <Metal/MTLDevice.h>

using namespace Tempest;
using namespace Tempest::Detail;

MtDevice::MtDevice() {
  impl  = NsPtr((__bridge void*)MTLCreateSystemDefaultDevice());

  id<MTLDevice> dx = impl.get();
  queue = NsPtr((__bridge void*)([dx newCommandQueue]));
  }

MtDevice::~MtDevice() {

  }

void MtDevice::waitIdle() {

  }
