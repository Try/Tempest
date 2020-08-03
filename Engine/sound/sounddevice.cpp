#include "sounddevice.h"

#include <AL/al.h>
#include <AL/alc.h>

#include <Tempest/File>
#include <Tempest/Sound>
#include <Tempest/SoundEffect>
#include <Tempest/Except>

#include <vector>
#include <mutex>

using namespace Tempest;

struct SoundDevice::Data {
  std::shared_ptr<Device> dev;
  ALCcontext*             context;
  };

struct SoundDevice::Device {
  Device() {
    dev = alcOpenDevice(nullptr);
    if(dev==nullptr)
      throw std::system_error(Tempest::SoundErrc::NoDevice);
    }
  ~Device(){
    alcCloseDevice(dev);
    }

  ALCdevice* dev=nullptr;
  };

SoundDevice::SoundDevice():data( new Data() ) {
  data->dev = device();

  data->context = alcCreateContext(data->dev->dev,nullptr);
  if(data->context==nullptr)
    throw std::system_error(Tempest::SoundErrc::NoDevice);

  alDistanceModel(data->context, AL_LINEAR_DISTANCE);
  process();
  }

SoundDevice::~SoundDevice() {
  if( data->context ){
    alcDestroyContext(data->context);
    }
  }

SoundEffect SoundDevice::load(const char *fname) {
  Tempest::Sound s(fname);
  return load(s);
  }

SoundEffect SoundDevice::load(IDevice &d) {
  Tempest::Sound s(d);
  return load(s);
  }

SoundEffect SoundDevice::load(const Sound &snd) {
  return SoundEffect(*this,snd);
  }

SoundEffect SoundDevice::load(std::unique_ptr<SoundProducer>&& p) {
  return SoundEffect(*this,std::move(p));
  }

void SoundDevice::process() {
  alcProcessContext(data->context);
  }

void SoundDevice::suspend() {
  alcSuspendContext(data->context);
  }

void SoundDevice::setListenerPosition(float x, float y, float z) {
  float xyz[]={x,y,z};
  alListenerfvCt(data->context,AL_POSITION,xyz);
  }

void SoundDevice::setListenerDirection(float dx,float dy,float dz,float ux,float uy,float uz) {
  float ori[6] = {dx,dy,dz, ux,uy,uz};
  alListenerfvCt(data->context,AL_ORIENTATION,ori);
  }

void SoundDevice::setGlobalVolume(float v) {
  float fv[]={v};
  alListenerfvCt(data->context,AL_GAIN,fv);
  }

void* SoundDevice::context() {
  return data->context;
  }

std::shared_ptr<SoundDevice::Device> SoundDevice::device() {
  static std::mutex sync;
  std::lock_guard<std::mutex> guard(sync);
  return implDevice();
  }

std::shared_ptr<SoundDevice::Device> SoundDevice::implDevice() {
  static std::weak_ptr<SoundDevice::Device> val;
  if(auto v = val.lock()){
    return v;
    }
  auto vx = std::make_shared<Device>();
  val = vx;
  return vx;
  }
