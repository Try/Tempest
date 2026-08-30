#if defined(TEMPEST_BUILD_METAL)

#include "mtprecompiledlibrary.h"

#include <Foundation/Foundation.hpp>
#include <dispatch/dispatch.h>

using namespace Tempest;
using namespace Tempest::Detail;

MtPrecompiledLibraries::MtPrecompiledLibraries(MTL::Device& device,
                                               const MetalApi::Options& options) {
  auto pool = NsPtr<NS::AutoreleasePool>::init();
  if(![(id)(void*)(&device) respondsToSelector:@selector(newLibraryWithData:error:)])
    return;

  for(const auto& src:options.precompiledLibraries) {
    if(src.data.empty() || src.shaders.empty())
      continue;
    if(MetalApi::precompiledLibraryHash(src.data.data(),src.data.size())!=src.dataHash)
      continue;

    bool hasEligibleEntry = false;
    for(const auto& shader:src.shaders)
      hasEligibleEntry |= valid(shader);
    if(!hasEligibleEntry)
      continue;

    dispatch_data_t data = dispatch_data_create(src.data.data(),src.data.size(),nullptr,
                                                DISPATCH_DATA_DESTRUCTOR_DEFAULT);
    if(data==nullptr)
      continue;

    NS::Error* err = nullptr;
    auto library = NsPtr<MTL::Library>(device.newLibrary(data,&err));
    dispatch_release(data);
    if(library==nullptr || err!=nullptr)
      continue;

    const size_t libraryIndex = libraries.size();
    libraries.push_back({std::move(library)});
    for(const auto& shader:src.shaders) {
      if(!valid(shader))
        continue;
      entries.push_back({shader.profile,shader.key,libraryIndex,false});
      }
    }

  for(size_t i=0; i<entries.size(); ++i) {
    for(size_t r=0; r<i; ++r) {
      if(entries[i].key!=entries[r].key)
        continue;
      entries[i].duplicate = true;
      entries[r].duplicate = true;
      }
    }
  }

NsPtr<MTL::Function> MtPrecompiledLibraries::find(
    std::string_view canonicalMsl,
    const MetalApi::PrecompiledShaderProfile& runtimeProfile) const {
  auto pool = NsPtr<NS::AutoreleasePool>::init();
  for(const auto& entry:entries) {
    if(entry.duplicate)
      continue;
    if(!sameGenerationProfile(entry.profile,runtimeProfile))
      continue;
    if(MetalApi::precompiledShaderKey(canonicalMsl,entry.profile)!=entry.key)
      continue;
    if(entry.library>=libraries.size() || libraries[entry.library].impl==nullptr)
      continue;

    auto name = NsPtr<NS::String>(NS::String::string(entry.profile.entryPoint.c_str(),NS::UTF8StringEncoding));
    if(name==nullptr)
      continue;
    name->retain();

    auto constants = NsPtr<MTL::FunctionConstantValues>::init();
    if(constants==nullptr)
      continue;
    NS::Error* err = nullptr;
    auto fn = NsPtr<MTL::Function>(libraries[entry.library].impl.get()->newFunction(name.get(),constants.get(),&err));
    if(fn==nullptr || err!=nullptr)
      continue;
    if(!sameStage(fn->functionType(),runtimeProfile.stage))
      continue;
    return fn;
    }
  return NsPtr<MTL::Function>();
  }

MetalApi::PrecompiledPlatform MtPrecompiledLibraries::currentPlatform() {
#if defined(__IOS__)
#if defined(TARGET_OS_SIMULATOR) && TARGET_OS_SIMULATOR
  return MetalApi::PrecompiledPlatform::IOSSimulator;
#else
  return MetalApi::PrecompiledPlatform::IOSDevice;
#endif
#else
  return MetalApi::PrecompiledPlatform::MacOS;
#endif
  }

bool MtPrecompiledLibraries::valid(const MetalApi::PrecompiledShader& shader) {
  const auto& profile = shader.profile;
  if(profile.schemaVersion!=MetalApi::PrecompiledShaderSchemaVersion ||
     profile.mslGeneratorVersion!=MetalApi::MslGeneratorVersion ||
     profile.platform!=currentPlatform() || profile.entryPoint.empty() ||
     profile.entryPoint.find('\0')!=std::string::npos || profile.mslVersion==0)
    return false;
  if(uint8_t(profile.stage)>uint8_t(MetalApi::PrecompiledShaderStage::Mesh) ||
     profile.argumentBuffersTier>2)
    return false;
  return true;
  }

bool MtPrecompiledLibraries::sameGenerationProfile(
    const MetalApi::PrecompiledShaderProfile& a,
    const MetalApi::PrecompiledShaderProfile& b) {
  return a.schemaVersion==b.schemaVersion &&
         a.mslGeneratorVersion==b.mslGeneratorVersion &&
         a.platform==b.platform && a.stage==b.stage &&
         a.entryPoint==b.entryPoint &&
         a.mslVersion==b.mslVersion && a.flipVertY==b.flipVertY &&
         a.bufferSizeBufferIndex==b.bufferSizeBufferIndex &&
         a.argumentBuffersTier==b.argumentBuffersTier &&
         a.runtimeArrayRichDescriptor==b.runtimeArrayRichDescriptor &&
         a.readWriteTextureFences==b.readWriteTextureFences &&
         a.nativeImageAtomics==b.nativeImageAtomics &&
         a.r32uiLinearTextureAlignment==b.r32uiLinearTextureAlignment &&
         a.r32uiAlignmentConstantId==b.r32uiAlignmentConstantId;
  }

bool MtPrecompiledLibraries::sameStage(MTL::FunctionType actual,
                                       MetalApi::PrecompiledShaderStage expected) {
  switch(expected) {
    case MetalApi::PrecompiledShaderStage::Vertex:
      return actual==MTL::FunctionTypeVertex;
    case MetalApi::PrecompiledShaderStage::Control:
      return actual==MTL::FunctionTypeKernel;
    case MetalApi::PrecompiledShaderStage::Evaluate:
      return actual==MTL::FunctionTypeVertex;
    case MetalApi::PrecompiledShaderStage::Geometry:
      return actual==MTL::FunctionTypeVertex;
    case MetalApi::PrecompiledShaderStage::Fragment:
      return actual==MTL::FunctionTypeFragment;
    case MetalApi::PrecompiledShaderStage::Compute:
      return actual==MTL::FunctionTypeKernel;
    case MetalApi::PrecompiledShaderStage::Task:
      return actual==MTL::FunctionTypeObject;
    case MetalApi::PrecompiledShaderStage::Mesh:
      return actual==MTL::FunctionTypeMesh;
    }
  return false;
  }

#endif
