#include "soundeffect.h"

#include <AL/al.h>
#include <AL/alc.h>

#include <Tempest/SoundDevice>

using namespace Tempest;

SoundEffect::SoundEffect(SoundDevice &dev, const Sound &src)
  :data(src.data){
  ctx = reinterpret_cast<ALCcontext*>(dev.context());
  if(data){
    ALCcontext* ctx = reinterpret_cast<ALCcontext*>(this->ctx);
    alGenSourcesCt(ctx,1, &source);
    alSourceiCt(ctx,source, AL_BUFFER, int(data->buffer));
    }
  }

Tempest::SoundEffect::SoundEffect(Tempest::SoundEffect &&s)
  :source(s.source),ctx(s.ctx),data(s.data){
  s.source=0;
  }

SoundEffect::~SoundEffect() {
  if(source) {
    ALCcontext* ctx = reinterpret_cast<ALCcontext*>(this->ctx);
    alDeleteSourcesCt(ctx,1, &source);
    }
  }

Tempest::SoundEffect &Tempest::SoundEffect::operator=(Tempest::SoundEffect &&s){
  std::swap(source,s.source);
  std::swap(ctx,   s.ctx);
  std::swap(data,  s.data);
  return *this;
  }

void SoundEffect::play() {
  if(source==0)
    return;
  ALCcontext* ctx = reinterpret_cast<ALCcontext*>(this->ctx);
  alSourcePlayvCt(ctx,1,&source);
  }

void SoundEffect::pause() {
  if(source==0)
    return;
  ALCcontext* ctx = reinterpret_cast<ALCcontext*>(this->ctx);
  alSourcePausevCt(ctx,1,&source);
  }

bool SoundEffect::isEmpty() const {
  return data==nullptr;
  }

bool SoundEffect::isFinished() const {
  if(source==0)
    return true;
  int32_t state=0;
  ALCcontext* ctx = reinterpret_cast<ALCcontext*>(this->ctx);
  alGetSourceivCt(ctx,source,AL_SOURCE_STATE,&state);
  return state==AL_STOPPED;
  }

uint64_t Tempest::SoundEffect::timeLength() const {
  if(data)
    return data->timeLength();
  return 0;
  }

uint64_t Tempest::SoundEffect::currentTime() const {
  if(source==0)
    return 0;
  float result=0;
  ALCcontext* ctx = reinterpret_cast<ALCcontext*>(this->ctx);
  alGetSourcefvCt(ctx, source, AL_SEC_OFFSET, &result);
  return uint64_t(result*1000);
  }

void SoundEffect::setPosition(float x, float y, float z) {
  if(source==0)
    return;
  float p[3]={x,y,z};
  ALCcontext* ctx = reinterpret_cast<ALCcontext*>(this->ctx);
  alSourcefvCt(ctx, source, AL_POSITION, p);
  }

std::array<float,3> SoundEffect::position() const {
  std::array<float,3> ret={};
  if(source==0)
    return ret;
  ALCcontext* ctx = reinterpret_cast<ALCcontext*>(this->ctx);
  alGetSourcefvCt(ctx,source,AL_POSITION,&ret[0]);
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
  if(source==0)
    return;
  ALCcontext* ctx = reinterpret_cast<ALCcontext*>(this->ctx);
  alSourcefvCt(ctx, source, AL_MAX_DISTANCE,       &dist);
  }

void SoundEffect::setRefDistance(float dist) {
  if(source==0)
    return;
  ALCcontext* ctx = reinterpret_cast<ALCcontext*>(this->ctx);
  alSourcefvCt(ctx, source, AL_REFERENCE_DISTANCE, &dist);
  }

void SoundEffect::setVolume(float val) {
  if(source==0)
    return;
  ALCcontext* ctx = reinterpret_cast<ALCcontext*>(this->ctx);
  alSourcefvCt(ctx, source, AL_GAIN, &val);
  }

float SoundEffect::volume() const {
  if(source==0)
    return 0;
  float val=0;
  ALCcontext* ctx = reinterpret_cast<ALCcontext*>(this->ctx);
  alGetSourcefvCt(ctx, source, AL_GAIN, &val);
  return val;
  }
