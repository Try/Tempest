#include "shader.h"

#include <Tempest/Device>
#include <algorithm>

using namespace Tempest;

Shader::Shader(Device &dev, AbstractGraphicsApi::Shader *impl)
  :dev(&dev),impl(impl) {
  }

Shader::~Shader(){
  if(dev)
    dev->destroy(*this);
  }
