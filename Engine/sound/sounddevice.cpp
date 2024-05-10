#include "sounddevice.h"

#if defined(TEMPEST_BUILD_AUDIO)

#include <AL/al.h>
#include <AL/alc.h>
#include <AL/alext.h>

// private ext
#define AL_STOP_SOURCES_ON_DISCONNECT_SOFT       0x19AB

#include <Tempest/File>
#include <Tempest/Sound>
#include <Tempest/SoundEffect>
#include <Tempest/Except>
#include <Tempest/Log>

#include <vector>
#include <mutex>
#include <cstring>

using namespace Tempest;

enum class LogLevel {
  Disable,
  Error,
  Warning,
  Trace
  };
extern LogLevel gLogLevel; // HACK: openal spams too much

struct SoundDevice::Data {
  std::shared_ptr<Device> dev;
  ALCcontext*             context;
  };

struct SoundDevice::Device {
  Device(const char* deviceName) {
    gLogLevel = LogLevel::Error;
    // gLogLevel = LogLevel::Trace;
    dev = alcOpenDevice(deviceName);
    if(dev==nullptr)
      throw std::system_error(Tempest::SoundErrc::NoDevice);
    // dummy context for buffers
    ctx = alcCreateContext(dev,nullptr);
    if(ctx==nullptr) {
      alcCloseDevice(dev);
      throw std::system_error(Tempest::SoundErrc::NoDevice);
      }
    }
  ~Device(){
    alcDestroyContext(ctx);
    alcCloseDevice(dev);
    }

  ALCdevice*  dev = nullptr;
  ALCcontext* ctx = nullptr;
  };

SoundDevice::SoundDevice():SoundDevice("") {
  }

SoundDevice::SoundDevice(std::string_view name):data(new Data()) {
  data->dev = device();

  data->context = alcCreateContext(data->dev->dev,nullptr);
  if(data->context==nullptr)
    throw std::system_error(Tempest::SoundErrc::NoDevice);

  alcSetThreadContext(data->context);

  ALenum e[] = {AL_EVENT_TYPE_DISCONNECTED_SOFT};
  alEventControlSOFT(1, e, true);

  alEventCallbackSOFT([](ALenum eventType, ALuint object, ALuint param,
                         ALsizei length, const ALchar *message,
                         void *userParam) {
    auto& data = *reinterpret_cast<const Data*>(userParam);
    alcReopenDeviceSOFT(data.dev->dev, nullptr, nullptr);
    /*
    auto s    = alcReopenDeviceSOFT(data.dev->dev, nullptr, nullptr);
    auto name = alcGetString(data.dev->dev, ALC_ALL_DEVICES_SPECIFIER);
    Log::d("reopen sound device: ", name, " ret: ", (s ? "true" : "false"));
    */
    }, data.get());

  alDistanceModel(AL_LINEAR_DISTANCE);
  alDisable(AL_STOP_SOURCES_ON_DISCONNECT_SOFT);
  // TODO: api
  alListenerf(AL_METERS_PER_UNIT, 100.f);
  process();
  alcSetThreadContext(nullptr);
  }

SoundDevice::~SoundDevice() {
  if(data->context)
    alcDestroyContext(data->context);
  }

std::vector<SoundDevice::Props> SoundDevice::devices() {
  alcSetThreadContext(nullptr);
  const char* devices = alcGetString(NULL, ALC_ALL_DEVICES_SPECIFIER);

  std::vector<SoundDevice::Props> ret;
  while(devices!=nullptr && devices[0]!='\0') {
    Props dev;
    std::strncpy(dev.name, devices, sizeof(dev.name));
    ret.push_back(dev);
    devices += std::strlen(devices) + 1;
    }

  return ret;
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

void SoundDevice::setListenerPosition(const Vec3& p) {
  float xyz[] = {p.x,p.y,p.z};
  alcSetThreadContext(data->context);
  alListenerfv(AL_POSITION,xyz);
  alcSetThreadContext(nullptr);
  }

void SoundDevice::setListenerPosition(float x, float y, float z) {
  float xyz[] = {x,y,z};
  alcSetThreadContext(data->context);
  alListenerfv(AL_POSITION,xyz);
  alcSetThreadContext(nullptr);
  }

void SoundDevice::setListenerDirection(const Vec3& f, const Vec3& up) {
  setListenerDirection(f.x,f.y,f.z, up.x,up.y,up.z);
  }

void SoundDevice::setListenerDirection(float dx,float dy,float dz, float ux,float uy,float uz) {
  float ori[6] = {dx,dy,dz, ux,uy,uz};
  alcSetThreadContext(data->context);
  alListenerfv(AL_ORIENTATION,ori);
  alcSetThreadContext(nullptr);
  }

void SoundDevice::setGlobalVolume(float v) {
  alcSetThreadContext(data->context);
  alListenerf(AL_GAIN,v);
  alcSetThreadContext(nullptr);
  }

void* SoundDevice::context() {
  return data->context;
  }

std::unique_lock<std::mutex> SoundDevice::globalLock() {
  static std::mutex gil;
  std::unique_lock<std::mutex> g(gil);
  return g;
  }

void SoundDevice::setThreadContext() {
  auto d = device();
  alcSetThreadContext(d->ctx);
  }

std::shared_ptr<SoundDevice::Device> SoundDevice::device() {
  static std::mutex sync;
  std::lock_guard<std::mutex> guard(sync);
  return implDevice(nullptr);
  }

std::shared_ptr<SoundDevice::Device> SoundDevice::implDevice(const char* name) {
  static std::weak_ptr<SoundDevice::Device> val;
  if(auto v = val.lock()){
    return v;
    }
  auto vx = std::make_shared<Device>(name);
  val = vx;
  return vx;
  }

#endif
