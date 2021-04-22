#include "descriptorset.h"

#include <Tempest/Device>
#include <Tempest/UniformBuffer>

using namespace Tempest;

struct DescriptorSet::EmptyDesc : AbstractGraphicsApi::Desc {
  void set    (size_t,AbstractGraphicsApi::Texture*, const Sampler2d&){}
  void setSsbo(size_t,AbstractGraphicsApi::Texture*, uint32_t){}
  void setUbo (size_t,AbstractGraphicsApi::Buffer*,  size_t){}
  void setSsbo(size_t,AbstractGraphicsApi::Buffer*,  size_t){}
  void ssboBarriers(Detail::ResourceState&){}
  };

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
    impl.handler->set(layoutBind,tex.impl.handler,smp); else
    throw std::system_error(Tempest::GraphicsErrc::InvalidTexture);
  }

void DescriptorSet::set(size_t layoutBind, const Attachment& tex, const Sampler2d& smp) {
  if(tex.tImpl.impl.handler)
    impl.handler->set(layoutBind,tex.tImpl.impl.handler,smp); else
    throw std::system_error(Tempest::GraphicsErrc::InvalidTexture);
  }

void DescriptorSet::set(size_t layoutBind, const StorageImage& tex, uint32_t mipLevel) {
  if(tex.impl.handler)
    impl.handler->setSsbo(layoutBind,tex.impl.handler,mipLevel); else
    throw std::system_error(Tempest::GraphicsErrc::InvalidTexture);
  }

void DescriptorSet::set(size_t layoutBind, const Detail::ResourcePtr<Texture2d> &tex, const Sampler2d& smp) {
  if(tex.impl.handler)
    impl.handler->set(layoutBind,tex.impl.handler,smp); else
    throw std::system_error(Tempest::GraphicsErrc::InvalidTexture);
  }

void DescriptorSet::set(size_t layoutBind, const StorageBuffer& vbuf) {
  implBindSsbo(layoutBind,vbuf.impl,0);
  }

void DescriptorSet::set(size_t layoutBind, const StorageBuffer& vbuf, size_t offset) {
  implBindSsbo(layoutBind,vbuf.impl,offset);
  }

void DescriptorSet::implBindUbo(size_t layoutBind, const VideoBuffer& vbuf, size_t offsetBytes) {
  if(vbuf.impl.handler)
    impl.handler->setUbo(layoutBind,vbuf.impl.handler,offsetBytes); else
    throw std::system_error(Tempest::GraphicsErrc::InvalidUniformBuffer);
  }

void DescriptorSet::implBindSsbo(size_t layoutBind, const VideoBuffer& vbuf, size_t offsetBytes) {
  if(vbuf.impl.handler)
    impl.handler->setSsbo(layoutBind,vbuf.impl.handler,offsetBytes); else
    throw std::system_error(Tempest::GraphicsErrc::InvalidStorageBuffer);
  }
