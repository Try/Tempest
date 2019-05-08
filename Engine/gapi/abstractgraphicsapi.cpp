#include "abstractgraphicsapi.h"

using namespace Tempest;

bool AbstractGraphicsApi::Caps::hasSamplerFormat(TextureFormat f) const {
  uint64_t  m = uint64_t(1) << uint64_t(f);
  return smpFormat&m;
  }

bool AbstractGraphicsApi::Caps::hasAttachFormat(TextureFormat f) const {
  uint64_t  m = uint64_t(1) << uint64_t(f);
  return attFormat&m;
  }
