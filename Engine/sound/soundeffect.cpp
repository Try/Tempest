#include "soundeffect.h"

#include <AL/al.h>
#include <AL/alc.h>

using namespace Tempest;

SoundEffect::SoundEffect(const Sound &src)
  :data(src.data){
  if(data){
    alGenSources(1, &source);
    alSourcei(source, AL_BUFFER, int(data->buffer));
    }
  }

Tempest::SoundEffect::SoundEffect(Tempest::SoundEffect &&s)
  :source(s.source),data(s.data){
  s.source=0;
  }

SoundEffect::~SoundEffect() {
  if(source)
    alDeleteSources(1, &source);
  }

Tempest::SoundEffect &Tempest::SoundEffect::operator=(Tempest::SoundEffect &&s){
  std::swap(source,s.source);
  std::swap(data,s.data);
  return *this;
  }

void SoundEffect::play() {
  if(source==0)
    return;
  alSourcePlay(source);
  }

void SoundEffect::pause() {
  if(source==0)
    return;
  alSourcePause(source);
  }

uint64_t Tempest::SoundEffect::timeLength() const {
  if(data)
    return data->timeLength();
  return 0;
  }

uint64_t Tempest::SoundEffect::currentTime() const {
  float result=0;
  alGetSourcef(source, AL_SEC_OFFSET, &result);
  return uint64_t(result*1000);
  }
