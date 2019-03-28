#include "sounddevice.h"

#include <AL/al.h>
#include <AL/alc.h>

#include <Tempest/File>
#include <Tempest/Sound>

using namespace Tempest;

struct SoundDevice::Data {
  ALCdevice*  dev;
  ALCcontext* context;
  };

SoundDevice::SoundDevice():data( new Data() ) {
  data->dev = alcOpenDevice(nullptr);
  //const ALint context_attribs[] = { ALC_FREQUENCY, 22050, 0 };

  data->context = alcCreateContext(data->dev,nullptr);
  alcMakeContextCurrent(data->context);
  process();
  }

SoundDevice::~SoundDevice() {
  alcMakeContextCurrent(nullptr);

  if( data->context )
    alcDestroyContext(data->context);

  alcCloseDevice(data->dev);
  }

Sound SoundDevice::load(const char *fname) {
  Tempest::RFile f(fname);
  return load(f);
  }

Sound SoundDevice::load(IDevice &d) {
  return Sound(d);
  }

void SoundDevice::process() {
  alcProcessContext(data->context);
  }

void SoundDevice::suspend() {
  alcSuspendContext(data->context);
  }

void* SoundDevice::device() {
  return alcGetContextsDevice(data->context);
  }

void SoundDevice::setDevice(void *device) {
  data->dev = reinterpret_cast<ALCdevice*>(device);
  }
