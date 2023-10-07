#include "soundeffect.h"

#if defined(TEMPEST_BUILD_AUDIO)

#include <AL/al.h>
#include <AL/alc.h>
#include <AL/alext.h>

#include <Tempest/SoundDevice>
#include <Tempest/Except>
#include <Tempest/Log>

#include <thread>
#include <atomic>

using namespace Tempest;

struct SoundEffect::Impl {
  enum {
    NUM_BUF = 3,
    BUFSZ   = 4096
    };

  Impl()=default;

  Impl(SoundDevice &dev, const Sound &src)
    :dev(&dev), data(src.data) {
    if(data==nullptr)
      return;
    ALCcontext* ctx = context();
    alcSetThreadContext(ctx);

    auto guard = SoundDevice::globalLock();
    alGenSources(1, &source);
    alSourcei(source, AL_BUFFER, data->buffer);
    alSourcei(source, AL_SOURCE_SPATIALIZE_SOFT, AL_TRUE);
    if(alGetError()!=AL_NO_ERROR) {
      alcSetThreadContext(nullptr);
      throw std::bad_alloc();
      }
    alcSetThreadContext(nullptr);
    }

  Impl(SoundDevice &dev, std::unique_ptr<SoundProducer> &&psrc)
    :dev(&dev), data(nullptr), producer(std::move(psrc)) {
    ALCcontext*    ctx = context();
    SoundProducer& src = *producer;

    ALsizei        freq    = src.frequency;
    ALenum         frm     = src.channels==2 ? AL_FORMAT_STEREO16 : AL_FORMAT_MONO16;

    alcSetThreadContext(ctx);

    auto guard = SoundDevice::globalLock();
    alGenSources(1, &source);
    if(alGetError()!=AL_NO_ERROR) {
      alcSetThreadContext(nullptr);
      throw std::bad_alloc();
      }

    alGenBuffers(1, &stream);
    alBufferCallbackSOFT(stream, frm, freq, bufferCallback, this);
    if(alGetError()!=AL_NO_ERROR) {
      alDeleteSources(1, &source);
      alcSetThreadContext(nullptr);
      throw std::bad_alloc();
      }

    alSourcei(source, AL_BUFFER, stream);
    alSourcef(source, AL_REFERENCE_DISTANCE, 0);
    alcSetThreadContext(nullptr);
    }

  ~Impl() {
    if(source==0)
      return;
    auto* ctx = context();
    alcSetThreadContext(ctx);

    alSourcePausev(1,&source);
    alDeleteSources(1, &source);
    if(stream!=0)
      alDeleteBuffers(1, &stream);
    alcSetThreadContext(nullptr);
    }

  static ALsizei bufferCallback(ALvoid *userptr, ALvoid *sampledata, ALsizei numbytes) {
    auto& self = *reinterpret_cast<Impl*>(userptr);
    self.renderSound(reinterpret_cast<int16_t*>(sampledata), numbytes/(sizeof(int16_t)*2));
    return numbytes;
    }

  void renderSound(int16_t* data, size_t sz) noexcept {
    auto& src = *producer;
    src.renderSound(data,sz);
    }

  ALCcontext* context(){
    return reinterpret_cast<ALCcontext*>(dev->context());
    }

  SoundDevice*                   dev    = nullptr;
  std::shared_ptr<Sound::Data>   data;
  uint32_t                       source = 0;
  uint32_t                       stream = 0;

  std::unique_ptr<SoundProducer> producer;
  };

SoundProducer::SoundProducer(uint16_t frequency, uint16_t channels)
  :frequency(frequency),channels(channels){
  if(channels!=1 && channels!=2)
    throw std::system_error(Tempest::SoundErrc::InvalidChannelsCount);
  }

SoundEffect::SoundEffect(SoundDevice &dev, const Sound &src)
  :impl(new Impl(dev,src)) {
  }

SoundEffect::SoundEffect(SoundDevice &dev, std::unique_ptr<SoundProducer> &&src)
  :impl(new Impl(dev,std::move(src))) {
  }

SoundEffect::SoundEffect()
  :impl(new Impl()){
  }

SoundEffect::SoundEffect(Tempest::SoundEffect &&s)
  :impl(std::move(s.impl)) {
  }

SoundEffect::~SoundEffect() {
  }

SoundEffect &SoundEffect::operator=(Tempest::SoundEffect &&s){
  std::swap(impl,s.impl);
  return *this;
  }

void SoundEffect::play() {
  if(impl->source==0)
    return;
  ALCcontext* ctx = impl->context();
  alcSetThreadContext(ctx);
  alSourcePlayv(1,&impl->source);
  alcSetThreadContext(nullptr);
  }

void SoundEffect::pause() {
  if(impl->source==0)
    return;
  ALCcontext* ctx = impl->context();
  alcSetThreadContext(ctx);
  alSourcePausev(1,&impl->source);
  alcSetThreadContext(nullptr);
  }

bool SoundEffect::isEmpty() const {
  return impl->source==0;
  }

bool SoundEffect::isFinished() const {
  if(impl->source==0)
    return true;
  int32_t state=0;
  ALCcontext* ctx = impl->context();
  alcSetThreadContext(ctx);
  alGetSourceiv(impl->source,AL_SOURCE_STATE,&state);
  alcSetThreadContext(nullptr);
  return state==AL_STOPPED || state==AL_INITIAL;
  }

uint64_t Tempest::SoundEffect::timeLength() const {
  if(impl->data)
    return impl->data->timeLength();
  return 0;
  }

uint64_t Tempest::SoundEffect::currentTime() const {
  if(impl->source==0)
    return 0;
  float result=0;
  ALCcontext* ctx = impl->context();
  alcSetThreadContext(ctx);
  alGetSourcefv(impl->source, AL_SEC_OFFSET, &result);
  alcSetThreadContext(nullptr);
  return uint64_t(result*1000);
  }

void SoundEffect::setPosition(const Vec3& p) {
  float v[3]={p.x,p.y,p.z};
  if(impl->source==0)
    return;
  ALCcontext* ctx = impl->context();
  alcSetThreadContext(ctx);
  alSourcefv(impl->source, AL_POSITION, v);
  alcSetThreadContext(nullptr);
  }

void SoundEffect::setPosition(float x, float y, float z) {
  float p[3]={x,y,z};
  if(impl->source==0)
    return;
  ALCcontext* ctx = impl->context();
  alcSetThreadContext(ctx);
  alSourcefv(impl->source, AL_POSITION, p);
  alcSetThreadContext(nullptr);
  }

Vec3 SoundEffect::position() const {
  float v[3] = {};
  if(impl->source==0)
    return Vec3();
  ALCcontext* ctx = impl->context();
  alcSetThreadContext(ctx);
  alGetSourcefv(impl->source,AL_POSITION,v);
  alcSetThreadContext(nullptr);
  return Vec3(v[0],v[1],v[2]);
  }

float SoundEffect::x() const {
  return position().x;
  }

float SoundEffect::y() const {
  return position().y;
  }

float SoundEffect::z() const {
  return position().z;
  }

void SoundEffect::setMaxDistance(float dist) {
  if(impl->source==0)
    return;
  ALCcontext* ctx = impl->context();
  alcSetThreadContext(ctx);
  alSourcefv(impl->source, AL_MAX_DISTANCE, &dist);
  alcSetThreadContext(nullptr);
  }

void SoundEffect::setVolume(float val) {
  if(impl->source==0)
    return;
  ALCcontext* ctx = impl->context();
  alcSetThreadContext(ctx);
  alSourcefv(impl->source, AL_GAIN, &val);
  alcSetThreadContext(nullptr);
  }

float SoundEffect::volume() const {
  if(impl->source==0)
    return 0;
  float val=0;
  ALCcontext* ctx = impl->context();
  alcSetThreadContext(ctx);
  alGetSourcefv(impl->source, AL_GAIN, &val);
  alcSetThreadContext(nullptr);
  return val;
  }

#endif
