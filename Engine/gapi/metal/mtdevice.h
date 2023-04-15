#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <Tempest/RenderState>
#include <Tempest/Except>

#include <Metal/Metal.hpp>
#include <Foundation/Foundation.hpp>

#include "../utility/compiller_hints.h"
#include "mtsamplercache.h"
#include "nsptr.h"

class MTLDevice;

namespace Tempest {
namespace Detail {

inline MTL::PixelFormat nativeFormat(TextureFormat frm) {
  switch(frm) {
    case Undefined:
    case Last:
      return MTL::PixelFormatInvalid;
    case R8:
      return MTL::PixelFormatR8Unorm;
    case RG8:
      return MTL::PixelFormatRG8Unorm;
    case RGB8:
      return MTL::PixelFormatInvalid;
    case RGBA8:
      return MTL::PixelFormatRGBA8Unorm;
    case R16:
      return MTL::PixelFormatR16Unorm;
    case RG16:
      return MTL::PixelFormatRG16Unorm;
    case RGB16:
      return MTL::PixelFormatInvalid;
    case RGBA16:
      return MTL::PixelFormatRGBA16Unorm;
    case R32F:
      return MTL::PixelFormatR32Float;
    case RG32F:
      return MTL::PixelFormatRG32Float;
    case RGB32F:
      return MTL::PixelFormatInvalid;
    case RGBA32F:
      return MTL::PixelFormatRGBA32Float;
    case R32U:
      return MTL::PixelFormatR32Uint;
    case RG32U:
      return MTL::PixelFormatRG32Uint;
    case RGB32U:
      return MTL::PixelFormatInvalid;
    case RGBA32U:
      return MTL::PixelFormatRGBA32Uint;
    case Depth16:
      return MTL::PixelFormatDepth16Unorm;
    case Depth24x8:
      return MTL::PixelFormatInvalid;
    case Depth24S8:
      return MTL::PixelFormatDepth24Unorm_Stencil8;
    case Depth32F:
      return MTL::PixelFormatDepth32Float;
    case DXT1:
      return MTL::PixelFormatBC1_RGBA;
    case DXT3:
      return MTL::PixelFormatBC2_RGBA;
    case DXT5:
      return MTL::PixelFormatBC3_RGBA;
    case R11G11B10UF:
      return MTL::PixelFormatRG11B10Float;
    case RGBA16F:
      return MTL::PixelFormatRGBA16Float;
    }
  return MTL::PixelFormatInvalid;
  }

inline MTL::VertexFormat nativeFormat(Decl::ComponentType t) {
  switch(t) {
    case Decl::ComponentType::count:
    case Decl::ComponentType::float0:
      return MTL::VertexFormatInvalid;
    case Decl::ComponentType::float1:
      return MTL::VertexFormatFloat;
    case Decl::ComponentType::float2:
      return MTL::VertexFormatFloat2;
    case Decl::ComponentType::float3:
      return MTL::VertexFormatFloat3;
    case Decl::ComponentType::float4:
      return MTL::VertexFormatFloat4;

    case Decl::ComponentType::int1:
      return MTL::VertexFormatInt;
    case Decl::ComponentType::int2:
      return MTL::VertexFormatInt2;
    case Decl::ComponentType::int3:
      return MTL::VertexFormatInt3;
    case Decl::ComponentType::int4:
      return MTL::VertexFormatInt4;

    case Decl::ComponentType::uint1:
      return MTL::VertexFormatUInt;
    case Decl::ComponentType::uint2:
      return MTL::VertexFormatUInt2;
    case Decl::ComponentType::uint3:
      return MTL::VertexFormatUInt3;
    case Decl::ComponentType::uint4:
      return MTL::VertexFormatUInt4;
    }
  return MTL::VertexFormatInvalid;
  }

inline MTL::CompareFunction nativeFormat(RenderState::ZTestMode m) {
  switch(m) {
    case RenderState::ZTestMode::Always:
      return MTL::CompareFunctionAlways;
    case RenderState::ZTestMode::Never:
      return MTL::CompareFunctionNever;
    case RenderState::ZTestMode::Greater:
      return MTL::CompareFunctionGreater;
    case RenderState::ZTestMode::Less:
      return MTL::CompareFunctionLess;
    case RenderState::ZTestMode::GEqual:
      return MTL::CompareFunctionGreaterEqual;
    case RenderState::ZTestMode::LEqual:
      return MTL::CompareFunctionLessEqual;
    case RenderState::ZTestMode::NOEqual:
      return MTL::CompareFunctionNotEqual;
    case RenderState::ZTestMode::Equal:
      return MTL::CompareFunctionEqual;
    }
  return MTL::CompareFunctionAlways;
  }

inline MTL::BlendFactor nativeFormat(RenderState::BlendMode m) {
  switch(m) {
    case RenderState::BlendMode::Zero:
      return MTL::BlendFactorZero;
    case RenderState::BlendMode::One:
      return MTL::BlendFactorOne;
    case RenderState::BlendMode::SrcColor:
      return MTL::BlendFactorSourceColor;
    case RenderState::BlendMode::OneMinusSrcColor:
      return MTL::BlendFactorOneMinusSourceColor;
    case RenderState::BlendMode::SrcAlpha:
      return MTL::BlendFactorSourceAlpha;
    case RenderState::BlendMode::SrcAlphaSaturate:
      return MTL::BlendFactorSourceAlphaSaturated;
    case RenderState::BlendMode::OneMinusSrcAlpha:
      return MTL::BlendFactorOneMinusSourceAlpha;
    case RenderState::BlendMode::DstColor:
      return MTL::BlendFactorDestinationColor;
    case RenderState::BlendMode::OneMinusDstColor:
      return MTL::BlendFactorOneMinusDestinationColor;
    case RenderState::BlendMode::DstAlpha:
      return MTL::BlendFactorDestinationAlpha;
    case RenderState::BlendMode::OneMinusDstAlpha:
      return MTL::BlendFactorOneMinusDestinationAlpha;
    }
  return MTL::BlendFactorZero;
  }

inline MTL::BlendOperation nativeFormat(RenderState::BlendOp op) {
  switch(op) {
    case RenderState::BlendOp::Add:
      return MTL::BlendOperationAdd;
    case RenderState::BlendOp::Max:
      return MTL::BlendOperationMax;
    case RenderState::BlendOp::Min:
      return MTL::BlendOperationMin;
    case RenderState::BlendOp::ReverseSubtract:
      return MTL::BlendOperationReverseSubtract;
    case RenderState::BlendOp::Subtract:
      return MTL::BlendOperationSubtract;
    }
  return MTL::BlendOperationAdd;
  }

inline MTL::CullMode nativeFormat(RenderState::CullMode m) {
  switch(m) {
    case RenderState::CullMode::NoCull:
      return MTL::CullModeNone;
    case RenderState::CullMode::Back:
      return MTL::CullModeBack;
    case RenderState::CullMode::Front:
      return MTL::CullModeFront;
    }
  return MTL::CullModeNone;
  }

inline MTL::PrimitiveType nativeFormat(Topology t) {
  switch(t) {
    case Topology::Points:    return MTL::PrimitiveTypePoint;
    case Topology::Lines:     return MTL::PrimitiveTypeLine;
    case Topology::Triangles: return MTL::PrimitiveTypeTriangle;
    }
  return MTL::PrimitiveTypePoint;
  }

inline MTL::IndexType nativeFormat(IndexClass icls) {
  switch(icls) {
    case IndexClass::i16: return MTL::IndexTypeUInt16;
    case IndexClass::i32: return MTL::IndexTypeUInt32;
    }
  return MTL::IndexTypeUInt16;
  }

class MtDevice : public AbstractGraphicsApi::Device {
  public:
    MtDevice(const char* name, bool validation);
    ~MtDevice();

    void waitIdle() override;

    static void handleError(NS::Error* err);

    struct autoDevice {
      autoDevice(const char* name);
      ~autoDevice();
      NsPtr<MTL::Device>       impl;
      NsPtr<MTL::CommandQueue> queue;
      };

    NsPtr<MTL::Device>         impl;
    NsPtr<MTL::CommandQueue>   queue;

    AbstractGraphicsApi::Props prop;
    MtSamplerCache             samplers;
    bool                       validation = false;

    static void deductProps(AbstractGraphicsApi::Props& prop, MTL::Device& dev);
  };

inline void mtAssert(void* obj, NS::Error* err) {
  if(T_LIKELY(obj!=nullptr))
    return;
  MtDevice::handleError(err);
  }

}
}
