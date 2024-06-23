#include "soundeffect.h"

#if defined(TEMPEST_BUILD_AUDIO)

#include <AL/al.h>
#include <AL/alc.h>
#include <AL/alext.h>

#include <Tempest/SoundDevice>
#include <Tempest/Except>
#include <Tempest/Log>

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

    auto guard = SoundDevice::globalLock(); // for alGetError
    alGenSourcesDirect(ctx, 1, &source);
    alSourceiDirect(ctx, source, AL_BUFFER, data->buffer);
    alSourceiDirect(ctx, source, AL_SOURCE_SPATIALIZE_SOFT, AL_TRUE);
    if(alGetErrorDirect(ctx)!=AL_NO_ERROR)
      throw std::bad_alloc();
    }

  Impl(SoundDevice &dev, std::unique_ptr<SoundProducer> &&psrc)
    :dev(&dev), data(nullptr), producer(std::move(psrc)) {
    SoundProducer& src    = *producer;
    ALsizei        freq   = src.frequency;
    ALenum         frm    = src.channels==2 ? AL_FORMAT_STEREO16 : AL_FORMAT_MONO16;
    ALCcontext*    ctx    = reinterpret_cast<ALCcontext*>(dev.context());
    ALCcontext*    bufCtx = reinterpret_cast<ALCcontext*>(dev.bufferContext());

    auto guard = SoundDevice::globalLock(); // for alGetError
    alGenBuffersDirect(bufCtx, 1, &stream);
    alcSetThreadContext(bufCtx);
    alBufferCallbackSOFT(stream, frm, freq, bufferCallback, this);
    alcSetThreadContext(nullptr);
    if(alGetErrorDirect(bufCtx)!=AL_NO_ERROR) {
      throw std::bad_alloc();
      }

    alGenSourcesDirect(ctx, 1, &source);
    if(alGetErrorDirect(ctx)!=AL_NO_ERROR) {
      alDeleteBuffersDirect(bufCtx, 1, &stream);
      throw std::bad_alloc();
      }

    alSourceiDirect(ctx, source, AL_BUFFER, stream);
    alSourcefDirect(ctx, source, AL_REFERENCE_DISTANCE, 0);
    }

  ~Impl() {
    if(source==0)
      return;
    auto* ctx    = reinterpret_cast<ALCcontext*>(dev->context());
    auto* bufCtx = reinterpret_cast<ALCcontext*>(dev->bufferContext());

    alSourcePausevDirect(ctx, 1, &source);
    alDeleteSourcesDirect(ctx, 1, &source);
    if(stream!=0)
      alDeleteBuffersDirect(bufCtx, 1, &stream);
    }

  static ALsizei bufferCallback(ALvoid *userptr, ALvoid *sampledata, ALsizei numbytes) noexcept {
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
  alSourcePlayvDirect(ctx, 1, &impl->source);
  }

void SoundEffect::pause() {
  if(impl->source==0)
    return;
  ALCcontext* ctx = impl->context();
  alSourcePausevDirect(ctx, 1, &impl->source);
  }

bool SoundEffect::isEmpty() const {
  return impl->source==0;
  }

bool SoundEffect::isFinished() const {
  if(impl->source==0)
    return true;
  int32_t state=0;
  ALCcontext* ctx = impl->context();
  alGetSourceivDirect(ctx, impl->source, AL_SOURCE_STATE, &state);
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
  alGetSourcefvDirect(ctx, impl->source, AL_SEC_OFFSET, &result);
  return uint64_t(result*1000);
  }

void SoundEffect::setPosition(const Vec3& p) {
  float v[3]={p.x,p.y,p.z};
  if(impl->source==0)
    return;
  ALCcontext* ctx = impl->context();
  alSourcefvDirect(ctx, impl->source, AL_POSITION, v);
  }

void SoundEffect::setPosition(float x, float y, float z) {
  float p[3]={x,y,z};
  if(impl->source==0)
    return;
  ALCcontext* ctx = impl->context();
  alSourcefvDirect(ctx, impl->source, AL_POSITION, p);
  }

Vec3 SoundEffect::position() const {
  float v[3] = {};
  if(impl->source==0)
    return Vec3();
  ALCcontext* ctx = impl->context();
  alGetSourcefvDirect(ctx, impl->source, AL_POSITION, v);
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
  alSourcefvDirect(ctx, impl->source, AL_MAX_DISTANCE, &dist);
  }

void SoundEffect::setVolume(float val) {
  if(impl->source==0)
    return;
  ALCcontext* ctx = impl->context();
  alSourcefvDirect(ctx, impl->source, AL_GAIN, &val);
  }

float SoundEffect::volume() const {
  if(impl->source==0)
    return 0;
  float val=0;
  ALCcontext* ctx = impl->context();
  alGetSourcefvDirect(ctx, impl->source, AL_GAIN, &val);
  return val;
  }

#endif
