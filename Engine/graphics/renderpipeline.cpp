#include "renderpipeline.h"

#include <Tempest/Device>

using namespace Tempest;

RenderPipeline::RenderPipeline(Device &dev, AbstractGraphicsApi::Pipeline *p, uint32_t w, uint32_t h)
  :dev(&dev),impl(p),vpW(w),vpH(h) {
  }

RenderPipeline::~RenderPipeline() {
  }

RenderPipeline &RenderPipeline::operator =(RenderPipeline&& other) {
  impl = std::move(other.impl);
  std::swap(dev,other.dev);
  vpW  = other.vpW;
  vpH  = other.vpH;
  return *this;
  }

uint32_t RenderPipeline::w() const {
  return vpW;
  }

uint32_t RenderPipeline::h() const {
  return vpH;
  }
