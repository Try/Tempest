#include "descriptorset.h"

#include <Tempest/Device>
#include <Tempest/UniformBuffer>
#include <Tempest/AccelerationStructure>

#include "utility/smallarray.h"

using namespace Tempest;

DescriptorSet::EmptyDesc DescriptorSet::emptyDesc;

DescriptorSet::DescriptorSet(AbstractGraphicsApi::Desc *desc)
  : impl(desc) {
  if(impl.handler==nullptr)
    impl = &emptyDesc;
  }

DescriptorSet::DescriptorSet(DescriptorSet&& u)
  : impl(std::move(u.impl)) {
  }

DescriptorSet::~DescriptorSet() {
  if(impl.handler==&emptyDesc)
    return;
  delete impl.handler;
  }

DescriptorSet& DescriptorSet::operator=(DescriptorSet&& u) {
  impl=std::move(u.impl);
  return *this;
  }

void DescriptorSet::set(size_t layoutBind, const Texture2d &tex, const Sampler2d& smp) {
  if(tex.impl.handler)
    impl.handler->set(layoutBind,tex.impl.handler,smp,uint32_t(-1)); else
    throw std::system_error(Tempest::GraphicsErrc::InvalidTexture);
  }

void DescriptorSet::set(size_t layoutBind, const Attachment& tex, const Sampler2d& smp) {
  if(tex.tImpl.impl.handler)
    impl.handler->set(layoutBind,tex.tImpl.impl.handler,smp,uint32_t(-1)); else
    throw std::system_error(Tempest::GraphicsErrc::InvalidTexture);
  }

void DescriptorSet::set(size_t layoutBind, const StorageImage& tex, const Sampler2d& smp, uint32_t mipLevel) {
  if(tex.impl.handler)
    impl.handler->set(layoutBind,tex.impl.handler,smp,mipLevel); else
    throw std::system_error(Tempest::GraphicsErrc::InvalidTexture);
  }

void DescriptorSet::set(size_t layoutBind, const Detail::ResourcePtr<Texture2d> &tex, const Sampler2d& smp) {
  if(tex.impl.handler)
    impl.handler->set(layoutBind,tex.impl.handler,smp,uint32_t(-1)); else
    throw std::system_error(Tempest::GraphicsErrc::InvalidTexture);
  }

void DescriptorSet::set(size_t layoutBind, const std::vector<const Texture2d*>& tex) {
  Detail::SmallArray<AbstractGraphicsApi::Texture*,32> arr(tex.size());
  for(size_t i=0; i<tex.size(); ++i)
    arr[i] = tex[i]->impl.handler;
  impl.handler->set(layoutBind,arr.get(),tex.size(),Sampler2d::anisotrophy());
  }

void DescriptorSet::set(size_t layoutBind, const AccelerationStructure& tlas) {
  if(tlas.impl.handler)
    impl.handler->setTlas(layoutBind,tlas.impl.handler); else
    throw std::system_error(Tempest::GraphicsErrc::InvalidAccelerationStructure);
  }

void DescriptorSet::set(size_t layoutBind, const StorageBuffer& vbuf) {
  implBindSsbo(layoutBind,vbuf.impl,0);
  }

void DescriptorSet::set(size_t layoutBind, const StorageBuffer& vbuf, size_t offset) {
  implBindSsbo(layoutBind,vbuf.impl,offset);
  }

void DescriptorSet::implBindUbo(size_t layoutBind, const VideoBuffer& vbuf, size_t offsetBytes) {
  if(vbuf.impl.handler)
    impl.handler->set(layoutBind,vbuf.impl.handler,offsetBytes); else
    throw std::system_error(Tempest::GraphicsErrc::InvalidUniformBuffer);
  }

void DescriptorSet::implBindSsbo(size_t layoutBind, const VideoBuffer& vbuf, size_t offsetBytes) {
  if(vbuf.impl.handler)
    impl.handler->set(layoutBind,vbuf.impl.handler,offsetBytes); else
    throw std::system_error(Tempest::GraphicsErrc::InvalidStorageBuffer);
  }
