#include "uniformslayout.h"

using namespace Tempest;

UniformsLayout::UniformsLayout(Detail::DSharedPtr<AbstractGraphicsApi::UniformsLay*>&& impl)
  :impl(std::move(impl)) {
  }
