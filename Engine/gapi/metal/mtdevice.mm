#include "mtdevice.h"

#include <Metal/MTLDevice.h>

using namespace Tempest;
using namespace Tempest::Detail;

MtDevice::MtDevice() {
  impl = NsPtr((__bridge void*)MTLCreateSystemDefaultDevice());
  }

MtDevice::~MtDevice() {

  }

void MtDevice::waitIdle() {

  }
