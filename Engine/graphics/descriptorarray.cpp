#include "descriptorarray.h"

using namespace Tempest;

DescriptorArray::DescriptorArray(Device&, AbstractGraphicsApi::DescArray *desc)
  : impl(desc) {
  }

DescriptorArray::DescriptorArray(DescriptorArray&& u)
  : impl(std::move(u.impl)) {
  }

DescriptorArray::~DescriptorArray() {
  delete impl.handler;
  }

DescriptorArray& DescriptorArray::operator=(DescriptorArray&& u) {
  impl=std::move(u.impl);
  return *this;
  }
