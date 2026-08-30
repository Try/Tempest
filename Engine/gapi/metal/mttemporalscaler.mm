#include "mttemporalscaler.h"

#if defined(TEMPEST_BUILD_METALFX_TEMPORAL)

#include <Tempest/MetalApi>
#include <TargetConditionals.h>

#include <mutex>
#include <utility>

#include "mtdevice.h"
#include "mttexture.h"

using namespace Tempest;
using namespace Tempest::Detail;

namespace {

bool isMetalFxTemporalAvailable() {
#if TARGET_OS_IPHONE
  if(@available(iOS 16.0, *))
    return true;
#else
  if(@available(macOS 13.0, *))
    return true;
#endif
  return false;
  }

bool hasUsage(const MTL::Texture& texture, MTL::TextureUsage required) {
  return (texture.usage()&required)==required;
  }

bool hasFormatAndSize(const MTL::Texture& texture, MTL::PixelFormat format,
                      NS::UInteger width, NS::UInteger height) {
  return texture.textureType()==MTL::TextureType2D && texture.sampleCount()==1 &&
         texture.pixelFormat()==format && texture.width()==width && texture.height()==height;
  }

}

MtTemporalScaler::MtTemporalScaler(MtDevice& device, const TemporalScalerDesc& cfg)
  :owner(&device) {
  const auto inputFormat  = nativeFormat(cfg.inputFormat);
  const auto depthFormat  = nativeFormat(cfg.depthFormat);
  const auto motionFormat = nativeFormat(cfg.motionFormat);
  const auto outputFormat = nativeFormat(cfg.outputFormat);
  if(!isMetalFxTemporalAvailable() || inputFormat==MTL::PixelFormatInvalid ||
     depthFormat==MTL::PixelFormatInvalid || motionFormat==MTL::PixelFormatInvalid ||
     outputFormat==MTL::PixelFormatInvalid)
    return;
  if(cfg.inputWidth==0 || cfg.inputHeight==0 || cfg.outputWidth==0 || cfg.outputHeight==0)
    return;
  if(cfg.inputWidth>device.prop.tex2d.maxSize || cfg.inputHeight>device.prop.tex2d.maxSize ||
     cfg.outputWidth>device.prop.tex2d.maxSize || cfg.outputHeight>device.prop.tex2d.maxSize)
    return;

  auto pool = NsPtr<NS::AutoreleasePool>::init();
  if(!MTLFX::TemporalScalerDescriptor::supportsDevice(device.impl.get()))
    return;

  auto raw = MTLFX::TemporalScalerDescriptor::alloc();
  if(raw==nullptr)
    return;
  auto desc = NsPtr<MTLFX::TemporalScalerDescriptor>(raw->init());
  if(desc==nullptr)
    return;

  desc->setColorTextureFormat(inputFormat);
  desc->setDepthTextureFormat(depthFormat);
  desc->setMotionTextureFormat(motionFormat);
  desc->setOutputTextureFormat(outputFormat);
  desc->setInputWidth(cfg.inputWidth);
  desc->setInputHeight(cfg.inputHeight);
  desc->setOutputWidth(cfg.outputWidth);
  desc->setOutputHeight(cfg.outputHeight);
  desc->setAutoExposureEnabled(cfg.autoExposure);

  auto scaler = NsPtr<MTLFX::TemporalScaler>(desc->newTemporalScaler(device.impl.get()));
  if(scaler==nullptr)
    return;
  if(scaler->colorTextureFormat()!=inputFormat || scaler->depthTextureFormat()!=depthFormat ||
     scaler->motionTextureFormat()!=motionFormat || scaler->outputTextureFormat()!=outputFormat ||
     scaler->inputWidth()!=cfg.inputWidth || scaler->inputHeight()!=cfg.inputHeight ||
     scaler->outputWidth()!=cfg.outputWidth || scaler->outputHeight()!=cfg.outputHeight)
    return;
  impl = std::move(scaler);
  }

bool MtTemporalScaler::encode(MTL::CommandBuffer& cmd, MtTexture& input, MtTexture& depth,
                              MtTexture& motion, MtTexture& output, const TemporalScalerArgs& args) {
  std::lock_guard<std::mutex> guard(sync);
  if(impl==nullptr || input.impl==nullptr || depth.impl==nullptr ||
     motion.impl==nullptr || output.impl==nullptr)
    return false;
  if(cmd.device()!=owner->impl.get() || &input.dev!=owner || &depth.dev!=owner ||
     &motion.dev!=owner || &output.dev!=owner)
    return false;

  if(!hasFormatAndSize(*input.impl,impl->colorTextureFormat(),impl->inputWidth(),impl->inputHeight()) ||
     !hasFormatAndSize(*depth.impl,impl->depthTextureFormat(),impl->inputWidth(),impl->inputHeight()) ||
     !hasFormatAndSize(*motion.impl,impl->motionTextureFormat(),impl->inputWidth(),impl->inputHeight()) ||
     !hasFormatAndSize(*output.impl,impl->outputTextureFormat(),impl->outputWidth(),impl->outputHeight()))
    return false;
  if(output.impl->storageMode()!=MTL::StorageModePrivate)
    return false;

  if(!hasUsage(*input.impl,impl->colorTextureUsage()) ||
     !hasUsage(*depth.impl,impl->depthTextureUsage()) ||
     !hasUsage(*motion.impl,impl->motionTextureUsage()) ||
     !hasUsage(*output.impl,impl->outputTextureUsage()))
    return false;

  impl->setInputContentWidth(input.impl->width());
  impl->setInputContentHeight(input.impl->height());
  impl->setColorTexture(input.impl.get());
  impl->setDepthTexture(depth.impl.get());
  impl->setMotionTexture(motion.impl.get());
  impl->setOutputTexture(output.impl.get());
  impl->setJitterOffsetX(args.jitterOffsetX);
  impl->setJitterOffsetY(args.jitterOffsetY);
  impl->setMotionVectorScaleX(args.motionVectorScaleX);
  impl->setMotionVectorScaleY(args.motionVectorScaleY);
  impl->setReset(args.resetHistory);
  impl->setDepthReversed(args.depthReversed);
  impl->encodeToCommandBuffer(&cmd);
  return true;
  }

AbstractGraphicsApi::TemporalScaler*
  MetalApi::createTemporalScaler(AbstractGraphicsApi::Device* device, const TemporalScalerDesc& desc) {
  auto* dev = dynamic_cast<MtDevice*>(device);
  if(dev==nullptr)
    return nullptr;
  auto* scaler = new MtTemporalScaler(*dev,desc);
  if(!scaler->isValid()) {
    delete scaler;
    return nullptr;
    }
  return scaler;
  }

#endif
