#include "shader.h"

#include <Tempest/Device>
#include <algorithm>

using namespace Tempest;

Shader::Shader(Device &dev, Detail::DSharedPtr<AbstractGraphicsApi::Shader*>&& impl)
  :dev(&dev),impl(std::move(impl)) {
  }

Shader::~Shader(){
  }
