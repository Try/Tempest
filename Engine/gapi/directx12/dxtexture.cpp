#if defined(TEMPEST_BUILD_DIRECTX12)

#include "dxdevice.h"
#include "dxtexture.h"
#include "guid.h"

#include <Tempest/Except>

using namespace Tempest;
using namespace Tempest::Detail;

static int swizzle(ComponentSwizzle cs, int def) {
  switch(cs) {
    case ComponentSwizzle::Identity:
      return def;
    case ComponentSwizzle::R:
      return D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_0;
    case ComponentSwizzle::G:
      return D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_1;
    case ComponentSwizzle::B:
      return D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_2;
    case ComponentSwizzle::A:
      return D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_3;
      //case ComponentSwizzle::One:
      //  return D3D12_SHADER_COMPONENT_MAPPING_FORCE_VALUE_1;
    }
  return def;
  }

static UINT compMapping(ComponentMapping mapping) {
  // Describe and create a SRV for the texture.
  int mapv[4] = { swizzle(mapping.r,0),
                  swizzle(mapping.g,1),
                  swizzle(mapping.b,2),
                  swizzle(mapping.a,3) };
  return D3D12_ENCODE_SHADER_4_COMPONENT_MAPPING(mapv[0],mapv[1],mapv[2],mapv[3]);
  }

DxTexture::DxTexture(DxDevice& dev, ComPtr<ID3D12Resource>&& b, DXGI_FORMAT frm, NonUniqResId nonUniqId,
                     UINT mipCnt, UINT sliceCnt, bool is3D)
  : device(&dev), impl(std::move(b)), format(frm), nonUniqId(nonUniqId),
    mipCnt(mipCnt), sliceCnt(sliceCnt), is3D(is3D), isFilterable(false) {
  D3D12_FEATURE_DATA_FORMAT_SUPPORT ds = {};
  ds.Format = frm;
  if(SUCCEEDED(device->device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &ds, sizeof(ds)))) {
    isFilterable = (ds.Support1 & D3D12_FORMAT_SUPPORT1_SHADER_SAMPLE);
    }
  if(true) {
    // SRV seem to be allowed for any format, with some caveats around depth/stencil
    imgView = dev.descAlloc.allocHost(1);
    createView(dev.descAlloc.handle(imgView),dev,format,nullptr,uint32_t(-1),is3D);
    }
  }

DxTexture::DxTexture(DxTexture&& other) {
  std::swap(device,       other.device);
  std::swap(impl,         other.impl);
  std::swap(format,       other.format);
  std::swap(nonUniqId,    other.nonUniqId);
  std::swap(mipCnt,       other.mipCnt);
  std::swap(sliceCnt,     other.sliceCnt);
  std::swap(is3D,         other.is3D);
  std::swap(isFilterable, other.isFilterable);

  std::swap(extViews,     other.extViews);
  std::swap(imgView,      other.imgView);
  }

DxTexture::~DxTexture() {
  if(device==nullptr)
    return;
  device->descAlloc.free(imgView);
  for(auto& i:extViews)
    device->descAlloc.free(i.v);
  }

D3D12_CPU_DESCRIPTOR_HANDLE DxTexture::view(DxDevice& dev, const ComponentMapping& m, uint32_t mipLevel, bool is3D) {
  if(m.r==ComponentSwizzle::Identity &&
     m.g==ComponentSwizzle::Identity &&
     m.b==ComponentSwizzle::Identity &&
     m.a==ComponentSwizzle::Identity &&
     (mipLevel==uint32_t(-1) || mipCnt==1) &&
     this->is3D==is3D) {
    return device->descAlloc.handle(imgView);
    }

  std::lock_guard<Detail::SpinLock> guard(syncViews);
  for(auto& i:extViews) {
    if(i.m==m && i.mip==mipLevel && i.is3D==is3D)
      return device->descAlloc.handle(i.v);
    }

  View v;
  v.v = dev.descAlloc.allocHost(1);
  createView(dev.descAlloc.handle(v.v),dev,format,&m,mipLevel,is3D);
  v.m    = m;
  v.mip  = mipLevel;
  v.is3D = is3D;
  try {
    extViews.push_back(v);
    }
  catch (...) {
    device->descAlloc.free(v.v);
    throw;
    }
  return device->descAlloc.handle(v.v);
  }

void DxTexture::createView(D3D12_CPU_DESCRIPTOR_HANDLE ret, DxDevice& dev, DXGI_FORMAT format,
                           const ComponentMapping* cmap, uint32_t mipLevel, bool is3D) {
  ID3D12Device& device = *dev.device;

  D3D12_SHADER_RESOURCE_VIEW_DESC desc = {};
  desc.Shader4ComponentMapping = cmap!=nullptr ? compMapping(*cmap) : D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
  desc.Format                  = nativeSrvFormat(format);
  if(is3D) {
    desc.ViewDimension           = D3D12_SRV_DIMENSION_TEXTURE3D;
    desc.Texture3D.MipLevels     = mipCnt;
    //desc.Texture3D.WSize         = t.sliceCnt;
    if(mipLevel!=uint32_t(-1)) {
      desc.Texture3D.MostDetailedMip = mipLevel;
      desc.Texture3D.MipLevels       = 1;
      }
    } else {
    desc.ViewDimension           = D3D12_SRV_DIMENSION_TEXTURE2D;
    desc.Texture2D.MipLevels     = mipCnt;
    if(mipLevel!=uint32_t(-1)) {
      desc.Texture2D.MostDetailedMip = mipLevel;
      desc.Texture2D.MipLevels       = 1;
      }
    }

  device.CreateShaderResourceView(impl.get(), &desc, ret);
  }

UINT DxTexture::bitCount() const {
  switch(format) {
    case DXGI_FORMAT_UNKNOWN:
    case DXGI_FORMAT_FORCE_UINT:
    case DXGI_FORMAT_P208:
    case DXGI_FORMAT_V208:
    case DXGI_FORMAT_V408:
      break;
    case DXGI_FORMAT_R32G32B32A32_TYPELESS:
    case DXGI_FORMAT_R32G32B32A32_FLOAT:
    case DXGI_FORMAT_R32G32B32A32_UINT:
    case DXGI_FORMAT_R32G32B32A32_SINT:
      return 128;

    case DXGI_FORMAT_R32G32B32_TYPELESS:
    case DXGI_FORMAT_R32G32B32_FLOAT:
    case DXGI_FORMAT_R32G32B32_UINT:
    case DXGI_FORMAT_R32G32B32_SINT:
      return 96;

    case DXGI_FORMAT_R16G16B16A16_TYPELESS:
    case DXGI_FORMAT_R16G16B16A16_FLOAT:
    case DXGI_FORMAT_R16G16B16A16_UNORM:
    case DXGI_FORMAT_R16G16B16A16_UINT:
    case DXGI_FORMAT_R16G16B16A16_SNORM:
    case DXGI_FORMAT_R16G16B16A16_SINT:
    case DXGI_FORMAT_R32G32_TYPELESS:
    case DXGI_FORMAT_R32G32_FLOAT:
    case DXGI_FORMAT_R32G32_UINT:
    case DXGI_FORMAT_R32G32_SINT:
    case DXGI_FORMAT_R32G8X24_TYPELESS:
    case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
    case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
    case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
    case DXGI_FORMAT_Y416:
    case DXGI_FORMAT_Y210:
    case DXGI_FORMAT_Y216:
      return 64;

    case DXGI_FORMAT_R10G10B10A2_TYPELESS:
    case DXGI_FORMAT_R10G10B10A2_UNORM:
    case DXGI_FORMAT_R10G10B10A2_UINT:
    case DXGI_FORMAT_R11G11B10_FLOAT:
    case DXGI_FORMAT_R8G8B8A8_TYPELESS:
    case DXGI_FORMAT_R8G8B8A8_UNORM:
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
    case DXGI_FORMAT_R8G8B8A8_UINT:
    case DXGI_FORMAT_R8G8B8A8_SNORM:
    case DXGI_FORMAT_R8G8B8A8_SINT:
    case DXGI_FORMAT_R16G16_TYPELESS:
    case DXGI_FORMAT_R16G16_FLOAT:
    case DXGI_FORMAT_R16G16_UNORM:
    case DXGI_FORMAT_R16G16_UINT:
    case DXGI_FORMAT_R16G16_SNORM:
    case DXGI_FORMAT_R16G16_SINT:
    case DXGI_FORMAT_R32_TYPELESS:
    case DXGI_FORMAT_D32_FLOAT:
    case DXGI_FORMAT_R32_FLOAT:
    case DXGI_FORMAT_R32_UINT:
    case DXGI_FORMAT_R32_SINT:
    case DXGI_FORMAT_R24G8_TYPELESS:
    case DXGI_FORMAT_D24_UNORM_S8_UINT:
    case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
    case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
    case DXGI_FORMAT_R9G9B9E5_SHAREDEXP:
    case DXGI_FORMAT_R8G8_B8G8_UNORM:
    case DXGI_FORMAT_G8R8_G8B8_UNORM:
    case DXGI_FORMAT_B8G8R8A8_UNORM:
    case DXGI_FORMAT_B8G8R8X8_UNORM:
    case DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM:
    case DXGI_FORMAT_B8G8R8A8_TYPELESS:
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
    case DXGI_FORMAT_B8G8R8X8_TYPELESS:
    case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
    case DXGI_FORMAT_AYUV:
    case DXGI_FORMAT_Y410:
    case DXGI_FORMAT_YUY2:
    // case XBOX_DXGI_FORMAT_R10G10B10_7E3_A2_FLOAT:
    // case XBOX_DXGI_FORMAT_R10G10B10_6E4_A2_FLOAT:
    // case XBOX_DXGI_FORMAT_R10G10B10_SNORM_A2_UNORM:
      return 32;

    case DXGI_FORMAT_P010:
    case DXGI_FORMAT_P016:
    // case XBOX_DXGI_FORMAT_D16_UNORM_S8_UINT:
    // case XBOX_DXGI_FORMAT_R16_UNORM_X8_TYPELESS:
    // case XBOX_DXGI_FORMAT_X16_TYPELESS_G8_UINT:
    // case WIN10_DXGI_FORMAT_V408:
      return 24;

    case DXGI_FORMAT_R8G8_TYPELESS:
    case DXGI_FORMAT_R8G8_UNORM:
    case DXGI_FORMAT_R8G8_UINT:
    case DXGI_FORMAT_R8G8_SNORM:
    case DXGI_FORMAT_R8G8_SINT:
    case DXGI_FORMAT_R16_TYPELESS:
    case DXGI_FORMAT_R16_FLOAT:
    case DXGI_FORMAT_D16_UNORM:
    case DXGI_FORMAT_R16_UNORM:
    case DXGI_FORMAT_R16_UINT:
    case DXGI_FORMAT_R16_SNORM:
    case DXGI_FORMAT_R16_SINT:
    case DXGI_FORMAT_B5G6R5_UNORM:
    case DXGI_FORMAT_B5G5R5A1_UNORM:
    case DXGI_FORMAT_A8P8:
    case DXGI_FORMAT_B4G4R4A4_UNORM:
    // case WIN10_DXGI_FORMAT_P208:
    // case WIN10_DXGI_FORMAT_V208:
      return 16;

    case DXGI_FORMAT_NV12:
    case DXGI_FORMAT_420_OPAQUE:
    case DXGI_FORMAT_NV11:
      return 12;

    case DXGI_FORMAT_R8_TYPELESS:
    case DXGI_FORMAT_R8_UNORM:
    case DXGI_FORMAT_R8_UINT:
    case DXGI_FORMAT_R8_SNORM:
    case DXGI_FORMAT_R8_SINT:
    case DXGI_FORMAT_A8_UNORM:
    case DXGI_FORMAT_AI44:
    case DXGI_FORMAT_IA44:
    case DXGI_FORMAT_P8:
    // case XBOX_DXGI_FORMAT_R4G4_UNORM:
      return 8;

    case DXGI_FORMAT_R1_UNORM:
      return 1;

    case DXGI_FORMAT_BC1_TYPELESS:
    case DXGI_FORMAT_BC1_UNORM:
    case DXGI_FORMAT_BC1_UNORM_SRGB:
    case DXGI_FORMAT_BC4_TYPELESS:
    case DXGI_FORMAT_BC4_UNORM:
    case DXGI_FORMAT_BC4_SNORM:
      return 4;

    case DXGI_FORMAT_BC2_TYPELESS:
    case DXGI_FORMAT_BC2_UNORM:
    case DXGI_FORMAT_BC2_UNORM_SRGB:
    case DXGI_FORMAT_BC3_TYPELESS:
    case DXGI_FORMAT_BC3_UNORM:
    case DXGI_FORMAT_BC3_UNORM_SRGB:
    case DXGI_FORMAT_BC5_TYPELESS:
    case DXGI_FORMAT_BC5_UNORM:
    case DXGI_FORMAT_BC5_SNORM:
    case DXGI_FORMAT_BC6H_TYPELESS:
    case DXGI_FORMAT_BC6H_UF16:
    case DXGI_FORMAT_BC6H_SF16:
    case DXGI_FORMAT_BC7_TYPELESS:
    case DXGI_FORMAT_BC7_UNORM:
    case DXGI_FORMAT_BC7_UNORM_SRGB:
      return 8;
    }
  throw DeviceLostException();
  }

UINT DxTexture::bytePerBlockCount() const {
  switch(format) {
    case DXGI_FORMAT_UNKNOWN:
    case DXGI_FORMAT_FORCE_UINT:
      break;
    case DXGI_FORMAT_P208:
    case DXGI_FORMAT_V208:
    case DXGI_FORMAT_V408:
      return 1;
    case DXGI_FORMAT_R32G32B32A32_TYPELESS:
    case DXGI_FORMAT_R32G32B32A32_FLOAT:
    case DXGI_FORMAT_R32G32B32A32_UINT:
    case DXGI_FORMAT_R32G32B32A32_SINT:
      return 16;

    case DXGI_FORMAT_R32G32B32_TYPELESS:
    case DXGI_FORMAT_R32G32B32_FLOAT:
    case DXGI_FORMAT_R32G32B32_UINT:
    case DXGI_FORMAT_R32G32B32_SINT:
      return 12;

    case DXGI_FORMAT_R16G16B16A16_TYPELESS:
    case DXGI_FORMAT_R16G16B16A16_FLOAT:
    case DXGI_FORMAT_R16G16B16A16_UNORM:
    case DXGI_FORMAT_R16G16B16A16_UINT:
    case DXGI_FORMAT_R16G16B16A16_SNORM:
    case DXGI_FORMAT_R16G16B16A16_SINT:
    case DXGI_FORMAT_R32G32_TYPELESS:
    case DXGI_FORMAT_R32G32_FLOAT:
    case DXGI_FORMAT_R32G32_UINT:
    case DXGI_FORMAT_R32G32_SINT:
    case DXGI_FORMAT_R32G8X24_TYPELESS:
    case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
    case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
    case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
    case DXGI_FORMAT_Y416:
    case DXGI_FORMAT_Y210:
    case DXGI_FORMAT_Y216:
      return 8;

    case DXGI_FORMAT_R10G10B10A2_TYPELESS:
    case DXGI_FORMAT_R10G10B10A2_UNORM:
    case DXGI_FORMAT_R10G10B10A2_UINT:
    case DXGI_FORMAT_R11G11B10_FLOAT:
    case DXGI_FORMAT_R8G8B8A8_TYPELESS:
    case DXGI_FORMAT_R8G8B8A8_UNORM:
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
    case DXGI_FORMAT_R8G8B8A8_UINT:
    case DXGI_FORMAT_R8G8B8A8_SNORM:
    case DXGI_FORMAT_R8G8B8A8_SINT:
    case DXGI_FORMAT_R16G16_TYPELESS:
    case DXGI_FORMAT_R16G16_FLOAT:
    case DXGI_FORMAT_R16G16_UNORM:
    case DXGI_FORMAT_R16G16_UINT:
    case DXGI_FORMAT_R16G16_SNORM:
    case DXGI_FORMAT_R16G16_SINT:
    case DXGI_FORMAT_R32_TYPELESS:
    case DXGI_FORMAT_D32_FLOAT:
    case DXGI_FORMAT_R32_FLOAT:
    case DXGI_FORMAT_R32_UINT:
    case DXGI_FORMAT_R32_SINT:
    case DXGI_FORMAT_R24G8_TYPELESS:
    case DXGI_FORMAT_D24_UNORM_S8_UINT:
    case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
    case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
    case DXGI_FORMAT_R9G9B9E5_SHAREDEXP:
    case DXGI_FORMAT_R8G8_B8G8_UNORM:
    case DXGI_FORMAT_G8R8_G8B8_UNORM:
    case DXGI_FORMAT_B8G8R8A8_UNORM:
    case DXGI_FORMAT_B8G8R8X8_UNORM:
    case DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM:
    case DXGI_FORMAT_B8G8R8A8_TYPELESS:
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
    case DXGI_FORMAT_B8G8R8X8_TYPELESS:
    case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
    case DXGI_FORMAT_AYUV:
    case DXGI_FORMAT_Y410:
    case DXGI_FORMAT_YUY2:
    // case XBOX_DXGI_FORMAT_R10G10B10_7E3_A2_FLOAT:
    // case XBOX_DXGI_FORMAT_R10G10B10_6E4_A2_FLOAT:
    // case XBOX_DXGI_FORMAT_R10G10B10_SNORM_A2_UNORM:
      return 4;

    case DXGI_FORMAT_P010:
    case DXGI_FORMAT_P016:
    // case XBOX_DXGI_FORMAT_D16_UNORM_S8_UINT:
    // case XBOX_DXGI_FORMAT_R16_UNORM_X8_TYPELESS:
    // case XBOX_DXGI_FORMAT_X16_TYPELESS_G8_UINT:
    // case WIN10_DXGI_FORMAT_V408:
      return 2; // 10 bit?

    case DXGI_FORMAT_R8G8_TYPELESS:
    case DXGI_FORMAT_R8G8_UNORM:
    case DXGI_FORMAT_R8G8_UINT:
    case DXGI_FORMAT_R8G8_SNORM:
    case DXGI_FORMAT_R8G8_SINT:
    case DXGI_FORMAT_R16_TYPELESS:
    case DXGI_FORMAT_R16_FLOAT:
    case DXGI_FORMAT_D16_UNORM:
    case DXGI_FORMAT_R16_UNORM:
    case DXGI_FORMAT_R16_UINT:
    case DXGI_FORMAT_R16_SNORM:
    case DXGI_FORMAT_R16_SINT:
    case DXGI_FORMAT_B5G6R5_UNORM:
    case DXGI_FORMAT_B5G5R5A1_UNORM:
    case DXGI_FORMAT_A8P8:
    case DXGI_FORMAT_B4G4R4A4_UNORM:
    // case WIN10_DXGI_FORMAT_P208:
    // case WIN10_DXGI_FORMAT_V208:
      return 2;

    case DXGI_FORMAT_NV12:
    case DXGI_FORMAT_420_OPAQUE:
    case DXGI_FORMAT_NV11:
      return 2; // 12 bit

    case DXGI_FORMAT_R8_TYPELESS:
    case DXGI_FORMAT_R8_UNORM:
    case DXGI_FORMAT_R8_UINT:
    case DXGI_FORMAT_R8_SNORM:
    case DXGI_FORMAT_R8_SINT:
    case DXGI_FORMAT_A8_UNORM:
    case DXGI_FORMAT_AI44:
    case DXGI_FORMAT_IA44:
    case DXGI_FORMAT_P8:
    // case XBOX_DXGI_FORMAT_R4G4_UNORM:
      return 1;

    case DXGI_FORMAT_R1_UNORM:
      return 1; // 1 bit format

    case DXGI_FORMAT_BC1_TYPELESS:
    case DXGI_FORMAT_BC1_UNORM:
    case DXGI_FORMAT_BC1_UNORM_SRGB:
    case DXGI_FORMAT_BC4_TYPELESS:
    case DXGI_FORMAT_BC4_UNORM:
    case DXGI_FORMAT_BC4_SNORM:
      return 8;

    case DXGI_FORMAT_BC2_TYPELESS:
    case DXGI_FORMAT_BC2_UNORM:
    case DXGI_FORMAT_BC2_UNORM_SRGB:
    case DXGI_FORMAT_BC3_TYPELESS:
    case DXGI_FORMAT_BC3_UNORM:
    case DXGI_FORMAT_BC3_UNORM_SRGB:
    case DXGI_FORMAT_BC5_TYPELESS:
    case DXGI_FORMAT_BC5_UNORM:
    case DXGI_FORMAT_BC5_SNORM:
    case DXGI_FORMAT_BC6H_TYPELESS:
    case DXGI_FORMAT_BC6H_UF16:
    case DXGI_FORMAT_BC6H_SF16:
    case DXGI_FORMAT_BC7_TYPELESS:
    case DXGI_FORMAT_BC7_UNORM:
    case DXGI_FORMAT_BC7_UNORM_SRGB:
      return 16;
    }
  throw DeviceLostException();
  }

DxTextureWithRT::DxTextureWithRT(DxDevice& dev, DxTexture&& base)
  :DxTexture(std::move(base)) {
  auto& device = *dev.device;

  D3D12_DESCRIPTOR_HEAP_DESC desc = {};
  desc.Type           = nativeIsDepthFormat(format) ? D3D12_DESCRIPTOR_HEAP_TYPE_DSV : D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
  desc.NumDescriptors = nativeIsDepthFormat(format) ? 2 : 1;
  desc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
  dxAssert(device.CreateDescriptorHeap(&desc, uuid<ID3D12DescriptorHeap>(), reinterpret_cast<void**>(&heap)));

  handle = heap->GetCPUDescriptorHandleForHeapStart();
  if(nativeIsDepthFormat(format)) {
    const auto dsvHeapInc = dev.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    device.CreateDepthStencilView(impl.get(), nullptr, handle);

    handleR      = heap->GetCPUDescriptorHandleForHeapStart();
    handleR.ptr += dsvHeapInc;

    D3D12_DEPTH_STENCIL_VIEW_DESC dsv = {};
    dsv.Format             = format;
    dsv.ViewDimension      = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsv.Flags              = D3D12_DSV_FLAG_READ_ONLY_DEPTH;
    dsv.Texture2D.MipSlice = 0;
    device.CreateDepthStencilView(impl.get(), &dsv, handleR);
    } else {
    device.CreateRenderTargetView(impl.get(), nullptr, handle);
    }
  }

#endif
