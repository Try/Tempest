#include "descriptorset.h"

#include <Tempest/Device>
#include <Tempest/AccelerationStructure>
#include <Tempest/UniformBuffer>
#include <Tempest/StorageBuffer>
#include <Tempest/StorageImage>

#include "utility/smallarray.h"

#include <cassert>

using namespace Tempest;

AbstractGraphicsApi::EmptyDesc DescriptorSet::emptyDesc;

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

void DescriptorSet::set(size_t layoutBind, const Texture2d &tex, const Sampler& smp) {
  if(tex.impl.handler)
    impl.handler->set(layoutBind,tex.impl.handler,smp,uint32_t(-1)); else
    throw std::system_error(Tempest::GraphicsErrc::InvalidTexture);
  }

void DescriptorSet::set(size_t layoutBind, const Attachment& tex, const Sampler& smp) {
  auto& val = textureCast<const Texture2d&>(tex);
  if(val.impl.handler)
    impl.handler->set(layoutBind,val.impl.handler,smp,uint32_t(-1)); else
    throw std::system_error(Tempest::GraphicsErrc::InvalidTexture);
  }

void DescriptorSet::set(size_t layoutBind, const ZBuffer& tex, const Sampler& smp) {
  if(tex.tImpl.impl.handler)
    impl.handler->set(layoutBind,tex.tImpl.impl.handler,smp,uint32_t(-1)); else
    throw std::system_error(Tempest::GraphicsErrc::InvalidTexture);
  }

void DescriptorSet::set(size_t layoutBind, const StorageImage& tex, const Sampler& smp, uint32_t mipLevel) {
  if(tex.tImpl.impl.handler)
    impl.handler->set(layoutBind,tex.tImpl.impl.handler,smp,mipLevel); else
    throw std::system_error(Tempest::GraphicsErrc::InvalidTexture);
  }

void DescriptorSet::set(size_t layoutBind, const Sampler& smp) {
  // NOTE: separable samplers do not support mappings in native api
  assert(smp.mapping==ComponentMapping());
  impl.handler->set(layoutBind,smp);
  }

void DescriptorSet::set(size_t layoutBind, const Detail::ResourcePtr<Texture2d> &tex, const Sampler& smp) {
  if(tex.impl.handler)
    impl.handler->set(layoutBind,tex.impl.handler,smp,uint32_t(-1)); else
    throw std::system_error(Tempest::GraphicsErrc::InvalidTexture);
  }

void DescriptorSet::set(size_t layoutBind, const std::vector<const Texture2d*>& tex) {
  set(layoutBind,tex.data(),tex.size());
  }

void DescriptorSet::set(size_t layoutBind, const std::vector<const StorageBuffer*>& buf) {
  set(layoutBind,buf.data(),buf.size());
  }

void DescriptorSet::set(size_t layoutBind, const Texture2d* const * tex, size_t count) {
  Detail::SmallArray<AbstractGraphicsApi::Texture*,32> arr(count);
  for(size_t i=0; i<count; ++i)
    arr[i] = tex[i] ? tex[i]->impl.handler : nullptr;
  impl.handler->set(layoutBind,arr.get(),count,Sampler::nearest(),uint32_t(-1));
  }

void DescriptorSet::set(size_t layoutBind, const StorageBuffer* const* buf, size_t count) {
  Detail::SmallArray<AbstractGraphicsApi::Buffer*,32> arr(count);
  for(size_t i=0; i<count; ++i)
    arr[i] = buf[i] ? buf[i]->impl.impl.handler : nullptr;
  impl.handler->set(layoutBind,arr.get(),count);
  }

void DescriptorSet::set(size_t layoutBind, const AccelerationStructure& tlas) {
  if(tlas.impl.handler)
    impl.handler->set(layoutBind,tlas.impl.handler); else
    throw std::system_error(Tempest::GraphicsErrc::InvalidAccelerationStructure);
  }

void DescriptorSet::set(size_t layoutBind, const StorageBuffer& vbuf) {
  implBindSsbo(layoutBind,vbuf.impl,0);
  }

void DescriptorSet::set(size_t layoutBind, const StorageBuffer& vbuf, size_t offset) {
  implBindSsbo(layoutBind,vbuf.impl,offset);
  }

void DescriptorSet::implBindUbo(size_t layoutBind, const Detail::VideoBuffer& vbuf) {
  if(vbuf.impl.handler)
    impl.handler->set(layoutBind,vbuf.impl.handler,0); else
    throw std::system_error(Tempest::GraphicsErrc::InvalidUniformBuffer);
  }

void DescriptorSet::implBindSsbo(size_t layoutBind, const Detail::VideoBuffer& vbuf, size_t offset) {
  if(vbuf.impl.handler==nullptr && offset!=0)
    throw std::system_error(Tempest::GraphicsErrc::InvalidUniformBuffer);
  impl.handler->set(layoutBind,vbuf.impl.handler,offset);
  }
