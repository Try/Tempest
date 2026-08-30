#if defined(TEMPEST_BUILD_METALFX)

#include "mtspatialscaler.h"

#include <Tempest/MetalApi>
#include <TargetConditionals.h>

#include <mutex>

#include "mtdevice.h"
#include "mttexture.h"

using namespace Tempest;
using namespace Tempest::Detail;

namespace {

MTLFX::SpatialScalerColorProcessingMode nativeColorMode(SpatialScalerColorMode mode) {
  switch(mode) {
    case SpatialScalerColorMode::Perceptual:
      return MTLFX::SpatialScalerColorProcessingModePerceptual;
    case SpatialScalerColorMode::Linear:
      return MTLFX::SpatialScalerColorProcessingModeLinear;
    case SpatialScalerColorMode::HDR:
      return MTLFX::SpatialScalerColorProcessingModeHDR;
    }
  return MTLFX::SpatialScalerColorProcessingModePerceptual;
  }

bool isMetalFxAvailable() {
#if TARGET_OS_IPHONE
  if(@available(iOS 16.0, *))
    return true;
#else
  if(@available(macOS 13.0, *))
    return true;
#endif
  return false;
  }

}

MtSpatialScaler::MtSpatialScaler(MtDevice& device, const SpatialScalerDesc& cfg)
  :owner(&device) {
  const auto inputFormat  = nativeFormat(cfg.inputFormat);
  const auto outputFormat = nativeFormat(cfg.outputFormat);
  if(!isMetalFxAvailable() || inputFormat==MTL::PixelFormatInvalid || outputFormat==MTL::PixelFormatInvalid)
    return;
  if(cfg.inputWidth==0 || cfg.inputHeight==0 || cfg.outputWidth==0 || cfg.outputHeight==0)
    return;

  auto pool = NsPtr<NS::AutoreleasePool>::init();
  if(!MTLFX::SpatialScalerDescriptor::supportsDevice(device.impl.get()))
    return;

  auto raw  = MTLFX::SpatialScalerDescriptor::alloc();
  if(raw==nullptr)
    return;
  auto desc = NsPtr<MTLFX::SpatialScalerDescriptor>(raw->init());
  if(desc==nullptr)
    return;

  desc->setColorTextureFormat(inputFormat);
  desc->setOutputTextureFormat(outputFormat);
  desc->setInputWidth(cfg.inputWidth);
  desc->setInputHeight(cfg.inputHeight);
  desc->setOutputWidth(cfg.outputWidth);
  desc->setOutputHeight(cfg.outputHeight);
  desc->setColorProcessingMode(nativeColorMode(cfg.colorMode));

  impl = NsPtr<MTLFX::SpatialScaler>(desc->newSpatialScaler(device.impl.get()));
  }

bool MtSpatialScaler::encode(MTL::CommandBuffer& cmd, MtTexture& input, MtTexture& output) {
  std::lock_guard<std::mutex> guard(sync);
  if(impl==nullptr || input.impl==nullptr || output.impl==nullptr)
    return false;
  if(cmd.device()!=owner->impl.get() || &input.dev!=owner || &output.dev!=owner)
    return false;
  if(output.impl->storageMode()!=MTL::StorageModePrivate)
    return false;
  if(input.impl->pixelFormat()!=impl->colorTextureFormat() ||
     output.impl->pixelFormat()!=impl->outputTextureFormat())
    return false;
  if(input.impl->width()!=impl->inputWidth() || input.impl->height()!=impl->inputHeight() ||
     output.impl->width()!=impl->outputWidth() || output.impl->height()!=impl->outputHeight())
    return false;

  const auto inputUsage  = impl->colorTextureUsage();
  const auto outputUsage = impl->outputTextureUsage();
  if((input.impl->usage()&inputUsage)!=inputUsage ||
     (output.impl->usage()&outputUsage)!=outputUsage)
    return false;

  impl->setInputContentWidth(input.impl->width());
  impl->setInputContentHeight(input.impl->height());
  impl->setColorTexture(input.impl.get());
  impl->setOutputTexture(output.impl.get());
  impl->encodeToCommandBuffer(&cmd);
  return true;
  }

AbstractGraphicsApi::SpatialScaler*
  MetalApi::createSpatialScaler(AbstractGraphicsApi::Device* device, const SpatialScalerDesc& desc) {
  auto* dev = dynamic_cast<MtDevice*>(device);
  if(dev==nullptr)
    return nullptr;
  auto* scaler = new MtSpatialScaler(*dev,desc);
  if(!scaler->isValid()) {
    delete scaler;
    return nullptr;
    }
  return scaler;
  }

#endif
