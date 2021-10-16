#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <Tempest/RenderState>
#include <Tempest/Except>

#include "../utility/compiller_hints.h"
#include "mtsamplercache.h"
#import  <Metal/MTLPixelFormat.h>
#import  <Metal/MTLVertexDescriptor.h>
#import  <Metal/MTLDepthStencil.h>
#import  <Metal/MTLRenderPipeline.h>
#import  <Foundation/NSError.h>

class MTLDevice;

namespace Tempest {
namespace Detail {

inline MTLPixelFormat nativeFormat(TextureFormat frm) {
  switch(frm) {
    case Undefined:
    case Last:
      return MTLPixelFormatInvalid;
    case R8:
      return MTLPixelFormatR8Unorm;
    case RG8:
      return MTLPixelFormatRG8Unorm;
    case RGB8:
      return MTLPixelFormatInvalid;
    case RGBA8:
      return MTLPixelFormatRGBA8Unorm;
    case R16:
      return MTLPixelFormatR16Unorm;
    case RG16:
      return MTLPixelFormatRG16Unorm;
    case RGB16:
      return MTLPixelFormatInvalid;
    case RGBA16:
      return MTLPixelFormatRGBA16Unorm;
    case R32F:
      return MTLPixelFormatR32Float;
    case RG32F:
      return MTLPixelFormatRG32Float;
    case RGB32F:
      return MTLPixelFormatInvalid;
    case RGBA32F:
      return MTLPixelFormatRGBA32Float;
    case Depth16:
      return MTLPixelFormatDepth16Unorm;
    case Depth24x8:
      return MTLPixelFormatInvalid;
    case Depth24S8:
      return MTLPixelFormatDepth24Unorm_Stencil8;
    case DXT1:
      return MTLPixelFormatBC1_RGBA;
    case DXT3:
      return MTLPixelFormatBC2_RGBA;
    case DXT5:
      return MTLPixelFormatBC3_RGBA;
    }
  return MTLPixelFormatInvalid;
  }

inline MTLVertexFormat nativeFormat(Decl::ComponentType t) {
  switch(t) {
    case Decl::ComponentType::count:
    case Decl::ComponentType::float0:
      return MTLVertexFormatInvalid;
    case Decl::ComponentType::float1:
      return MTLVertexFormatFloat;
    case Decl::ComponentType::float2:
      return MTLVertexFormatFloat2;
    case Decl::ComponentType::float3:
      return MTLVertexFormatFloat3;
    case Decl::ComponentType::float4:
      return MTLVertexFormatFloat4;

    case Decl::ComponentType::int1:
      return MTLVertexFormatInt;
    case Decl::ComponentType::int2:
      return MTLVertexFormatInt2;
    case Decl::ComponentType::int3:
      return MTLVertexFormatInt3;
    case Decl::ComponentType::int4:
      return MTLVertexFormatInt4;

    case Decl::ComponentType::uint1:
      return MTLVertexFormatUInt;
    case Decl::ComponentType::uint2:
      return MTLVertexFormatUInt2;
    case Decl::ComponentType::uint3:
      return MTLVertexFormatUInt3;
    case Decl::ComponentType::uint4:
      return MTLVertexFormatUInt4;
    }
  return MTLVertexFormatInvalid;
  }

inline MTLCompareFunction nativeFormat(RenderState::ZTestMode m) {
  switch(m) {
    case RenderState::ZTestMode::Always:
      return MTLCompareFunctionAlways;
    case RenderState::ZTestMode::Never:
      return MTLCompareFunctionNever;
    case RenderState::ZTestMode::Greater:
      return MTLCompareFunctionGreater;
    case RenderState::ZTestMode::Less:
      return MTLCompareFunctionLess;
    case RenderState::ZTestMode::GEqual:
      return MTLCompareFunctionGreaterEqual;
    case RenderState::ZTestMode::LEqual:
      return MTLCompareFunctionLessEqual;
    case RenderState::ZTestMode::NOEqual:
      return MTLCompareFunctionNotEqual;
    case RenderState::ZTestMode::Equal:
      return MTLCompareFunctionEqual;
    case RenderState::ZTestMode::Count:
      return MTLCompareFunctionAlways;
    }
  return MTLCompareFunctionAlways;
  }

inline MTLBlendFactor nativeFormat(RenderState::BlendMode m) {
  switch(m) {
    case RenderState::BlendMode::zero:
    case RenderState::BlendMode::Count:
      return MTLBlendFactorZero;
    case RenderState::BlendMode::one:
      return MTLBlendFactorOne;
    case RenderState::BlendMode::src_color:
      return MTLBlendFactorSourceColor;
    case RenderState::BlendMode::one_minus_src_color:
      return MTLBlendFactorOneMinusSourceColor;
    case RenderState::BlendMode::src_alpha:
      return MTLBlendFactorSourceAlpha;
    case RenderState::BlendMode::src_alpha_saturate:
      return MTLBlendFactorSourceAlphaSaturated;
    case RenderState::BlendMode::one_minus_src_alpha:
      return MTLBlendFactorOneMinusSourceAlpha;
    case RenderState::BlendMode::dst_color:
      return MTLBlendFactorDestinationColor;
    case RenderState::BlendMode::one_minus_dst_color:
      return MTLBlendFactorOneMinusDestinationColor;
    case RenderState::BlendMode::dst_alpha:
      return MTLBlendFactorDestinationAlpha;
    case RenderState::BlendMode::one_minus_dst_alpha:
      return MTLBlendFactorOneMinusDestinationAlpha;
    }
  }

inline MTLBlendOperation nativeFormat(RenderState::BlendOp op) {
  switch(op) {
    case RenderState::BlendOp::Add:
      return MTLBlendOperationAdd;
    case RenderState::BlendOp::Max:
      return MTLBlendOperationMax;
    case RenderState::BlendOp::Min:
      return MTLBlendOperationMin;
    case RenderState::BlendOp::ReverseSubtract:
      return MTLBlendOperationReverseSubtract;
    case RenderState::BlendOp::Subtract:
      return MTLBlendOperationSubtract;
    }
  }

inline MTLCullMode nativeFormat(RenderState::CullMode m) {
  switch(m) {
    case RenderState::CullMode::NoCull:
    case RenderState::CullMode::Count:
      return MTLCullModeNone;
    case RenderState::CullMode::Back:
      return MTLCullModeBack;
    case RenderState::CullMode::Front:
      return MTLCullModeFront;
    }
  }

inline MTLPrimitiveType nativeFormat(Topology t) {
  switch(t) {
    case Topology::Lines:     return MTLPrimitiveTypeLine;
    case Topology::Triangles: return MTLPrimitiveTypeTriangle;
    }
  return MTLPrimitiveTypePoint;
  }

class MtDevice : public AbstractGraphicsApi::Device {
  public:
    MtDevice(const char* name);
    ~MtDevice();

    void waitIdle() override;

    static void handleError(NSError* err);

    struct autoDevice {
      autoDevice(const char* name);
      ~autoDevice();
      id<MTLDevice>       impl;
      id<MTLCommandQueue> queue;
      };

    id<MTLDevice>       impl;
    id<MTLCommandQueue> queue;

    AbstractGraphicsApi::Props prop;

    autoDevice     dev;
    MtSamplerCache samplers;

    static void deductProps(AbstractGraphicsApi::Props& prop, id<MTLDevice> dev);
  };

inline void mtAssert(id obj, NSError* err) {
  if(T_LIKELY(obj!=nil))
    return;
  MtDevice::handleError(err);
  }

}
}
