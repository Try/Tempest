#include "renderpass.h"

#include <Tempest/Device>

using namespace Tempest;

RenderPass::RenderPass(Detail::DSharedPtr<AbstractGraphicsApi::Pass *> &&impl)
  :impl(std::move(impl)) {
  }

RenderPass::~RenderPass() {
  }
