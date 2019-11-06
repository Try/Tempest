#include "soundeffect.h"

#include <AL/al.h>
#include <AL/alc.h>

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
    alGenSourcesCt(ctx, 1, &source);
    alSourceBufferCt(ctx, source, reinterpret_cast<ALbuffer*>(data->buffer));
    }

  Impl(SoundDevice &dev, std::unique_ptr<SoundProducer> &&src)
    :dev(&dev), data(nullptr), producer(std::move(src)) {
    ALCcontext* ctx = context();
    alGenSourcesCt(ctx, 1, &source);

    threadFlag.store(true);
    producerThread = std::thread([this](){
      soundThreadFn();
      });
    }

  ~Impl(){
    if(source==0)
      return;

    ALCcontext* ctx = context();
    if(threadFlag.load()) {
      alSourcePausevCt(ctx,1,&source);
      threadFlag.store(false);
      producerThread.join();
      }
    alDeleteSourcesCt(ctx, 1, &source);
    }

  void soundThreadFn() {
    SoundProducer& src  = *producer;
    ALsizei        freq = src.frequency;
    ALenum         frm  = src.channels==2 ? AL_FORMAT_STEREO16 : AL_FORMAT_MONO16;
    auto           ctx  = context();
    ALbuffer*      qBuffer[NUM_BUF]={};
    int16_t        bufData[BUFSZ*2]={};
    ALint          state=AL_INITIAL;
    ALint          zero=0;

    for(size_t i=0;i<NUM_BUF;++i){
      auto b = alNewBuffer();
      if(b==nullptr){
        for(size_t r=0;r<i;++r)
          alDelBuffer(b);
        return;
        }
      qBuffer[i] = b;
      }

    for(int i=0;i<NUM_BUF;++i) {
      renderSound(src,bufData,BUFSZ);
      alBufferDataCt(ctx, qBuffer[i], frm, bufData, BUFSZ*sizeof(int16_t)*2, freq);
      }
    alSourceQueueBuffersCt(ctx,source,NUM_BUF,qBuffer);
    alSourceivCt(ctx,source,AL_LOOPING,&zero);
    alSourcePlayvCt(ctx,1,&source);

    while(threadFlag.load()) {
      int bufInQueue=0, bufProcessed=0;
      alGetSourceivCt(ctx,source, AL_BUFFERS_QUEUED,    &bufInQueue);
      alGetSourceivCt(ctx,source, AL_BUFFERS_PROCESSED, &bufProcessed);

      if(bufInQueue>2 && bufProcessed==0){
        std::this_thread::yield();
        continue;
        }

      for(int i=0;i<bufProcessed;++i) {
        ALbuffer* nextBuffer=nullptr;
        alSourceUnqueueBuffersCt(ctx,source,1,&nextBuffer);

        renderSound(src,bufData,BUFSZ);
        alBufferDataCt(ctx, nextBuffer, frm, bufData, BUFSZ*sizeof(int16_t)*2, freq);
        alSourceQueueBuffersCt(ctx,source,1,&nextBuffer);
        }

      if(bufInQueue==1){
        uint64_t t = (uint64_t(1000u)*BUFSZ)/uint64_t(freq);
        std::this_thread::sleep_for(std::chrono::milliseconds(t));
        }

      alGetSourceivCt(ctx,source,AL_SOURCE_STATE,&state);
      if(state==AL_STOPPED)
        alSourcePlayvCt(ctx,1,&source); //HACK
      }

    for(size_t i=0;i<NUM_BUF;++i)
      alDelBuffer(qBuffer[i]);
    }

  void renderSound(SoundProducer& src,int16_t* data,size_t sz) noexcept {
    src.renderSound(data,sz);
    }

  ALCcontext* context(){
    return reinterpret_cast<ALCcontext*>(dev->context());
    }

  SoundDevice*                   dev    = nullptr;
  uint32_t                       source = 0;
  std::shared_ptr<Sound::Data>   data;

  std::thread                    producerThread;
  std::atomic_bool               threadFlag={false};
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
  alSourcePlayvCt(ctx,1,&impl->source);
  }

void SoundEffect::pause() {
  if(impl->source==0)
    return;
  ALCcontext* ctx = impl->context();
  alSourcePausevCt(ctx,1,&impl->source);
  }

bool SoundEffect::isEmpty() const {
  return impl->source==0;
  }

bool SoundEffect::isFinished() const {
  if(impl->source==0)
    return true;
  int32_t state=0;
  ALCcontext* ctx = impl->context();
  alGetSourceivCt(ctx,impl->source,AL_SOURCE_STATE,&state);
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
  alGetSourcefvCt(ctx, impl->source, AL_SEC_OFFSET, &result);
  return uint64_t(result*1000);
  }

void SoundEffect::setPosition(float x, float y, float z) {
  float p[3]={x,y,z};
  if(impl->source==0)
    return;
  ALCcontext* ctx = impl->context();
  alSourcefvCt(ctx, impl->source, AL_POSITION, p);
  }

std::array<float,3> SoundEffect::position() const {
  std::array<float,3> ret={};
  if(impl->source==0)
    return ret;
  ALCcontext* ctx = impl->context();
  alGetSourcefvCt(ctx,impl->source,AL_POSITION,&ret[0]);
  return ret;
  }

float SoundEffect::x() const {
  return position()[0];
  }

float SoundEffect::y() const {
  return position()[1];
  }

float SoundEffect::z() const {
  return position()[2];
  }

void SoundEffect::setMaxDistance(float dist) {
  if(impl->source==0)
    return;
  ALCcontext* ctx = impl->context();
  alSourcefvCt(ctx, impl->source, AL_MAX_DISTANCE, &dist);
  }

void SoundEffect::setRefDistance(float dist) {
  if(impl->source==0)
    return;
  ALCcontext* ctx = impl->context();
  alSourcefvCt(ctx, impl->source, AL_REFERENCE_DISTANCE, &dist);
  }

void SoundEffect::setVolume(float val) {
  if(impl->source==0)
    return;
  ALCcontext* ctx = impl->context();
  alSourcefvCt(ctx, impl->source, AL_GAIN, &val);
  }

float SoundEffect::volume() const {
  if(impl->source==0)
    return 0;
  float val=0;
  ALCcontext* ctx = impl->context();
  alGetSourcefvCt(ctx, impl->source, AL_GAIN, &val);
  return val;
  }
