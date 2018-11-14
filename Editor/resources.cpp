#include "resources.h"

Resources* Resources::inst=nullptr;

Resources::Resources(Tempest::Device &device)
  : asset("data",device) {
  inst=this;
  }

Resources::~Resources() {
  inst=nullptr;
  }
