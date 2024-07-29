#if defined(TEMPEST_BUILD_AUDIO)

#include "sounddevice.h"

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
  Device(std::string_view deviceNameSV) {
    std::string deviceName = std::string(deviceNameSV);
    gLogLevel = LogLevel::Error;
    // gLogLevel = LogLevel::Trace;
    dev = alcOpenDevice(deviceName.c_str());
    if(dev==nullptr)
      throw std::system_error(Tempest::SoundErrc::NoDevice);
    }
  ~Device(){
    alcCloseDevice(dev);
    }

  ALCdevice*  dev = nullptr;
  };

#if 0
struct SoundDevice::BufferContext {
  BufferContext(const std::shared_ptr<SoundDevice::Device>& dev):dev(dev) {
    ctx = alcCreateContext(dev->dev,nullptr);
    if(ctx==nullptr)
      throw std::system_error(Tempest::SoundErrc::NoDevice);
    }
  ~BufferContext() {
    alcDestroyContext(ctx);
    }

  std::shared_ptr<SoundDevice::Device> dev;
  ALCcontext*                          ctx = nullptr;
  };
#endif

struct SoundDevice::PhysicalDeviceList {
  std::mutex                         sync;
  std::weak_ptr<SoundDevice::Device> val;
#if 0
  BufferContext                      buf; // dummy context for buffers
#endif

  static PhysicalDeviceList& inst() {
    static PhysicalDeviceList list;
    return list;
    }

  PhysicalDeviceList()
#if 0
      : buf(device(""))
#endif
    {
    }

  ~PhysicalDeviceList(){}

  std::shared_ptr<SoundDevice::Device> device(std::string_view name) {
    std::lock_guard<std::mutex> guard(sync);
    if(auto v = val.lock()){
      return v;
      }
    auto vx = std::make_shared<Device>(name);
    val = vx;
    return vx;
    }
  };

SoundDevice::SoundDevice():SoundDevice("") {
  }

SoundDevice::SoundDevice(std::string_view name):data(new Data()) {
  data->dev = PhysicalDeviceList::inst().device(name);

  data->context = alcCreateContext(data->dev->dev,nullptr);
  if(data->context==nullptr)
    throw std::system_error(Tempest::SoundErrc::NoDevice);

  {
    ALenum e[] = {AL_EVENT_TYPE_DISCONNECTED_SOFT};
    alEventControlDirectSOFT(data->context, 1, e, true);

    alEventCallbackDirectSOFT(data->context,
        [](ALenum eventType, ALuint object, ALuint param,
           ALsizei length, const ALchar *message,
           void *userParam) noexcept {
          auto& data = *reinterpret_cast<const Data*>(userParam);
          alcReopenDeviceSOFT(data.dev->dev, nullptr, nullptr);
        }, data.get());
  }

  {
  alcSetThreadContext(data->context);
  ALenum e[] = {ALC_EVENT_TYPE_DEVICE_REMOVED_SOFT, ALC_EVENT_TYPE_DEVICE_ADDED_SOFT, ALC_EVENT_TYPE_DEFAULT_DEVICE_CHANGED_SOFT};
  alcEventControlSOFT(3, e, true);
  alcEventCallbackSOFT([](ALCenum eventType, ALCenum deviceType,
                          ALCdevice *device, ALCsizei length,
                          const ALCchar *message, void *userParam) noexcept {
    auto& data = *reinterpret_cast<const Data*>(userParam);
    if(deviceType!=ALC_PLAYBACK_DEVICE_SOFT)
      return;
    switch (eventType) {
      case ALC_EVENT_TYPE_DEFAULT_DEVICE_CHANGED_SOFT: {
        break;
        /*
        std::thread th([userParam]() {
          auto& data = *reinterpret_cast<const Data*>(userParam);
          auto s     = alcReopenDeviceSOFT(data.dev->dev, nullptr, nullptr);
          auto name  = alcGetString(data.dev->dev, ALC_ALL_DEVICES_SPECIFIER);
          Log::d("reopen sound device: ", name, " ret: ", (s ? "true" : "false"));
          });
        th.detach();
        */

        auto& data = *reinterpret_cast<const Data*>(userParam);
        auto s     = alcReopenDeviceSOFT(data.dev->dev, nullptr, nullptr);
        auto name  = alcGetString(data.dev->dev, ALC_ALL_DEVICES_SPECIFIER);
        Log::d("reopen sound device: ", name, " ret: ", (s ? "true" : "false"));

        //alcReopenDeviceSOFT(data.dev->dev, nullptr, nullptr);
        break;
        }
      case ALC_EVENT_TYPE_DEVICE_ADDED_SOFT:
        Log::d("reopen sound device: ");
        break;
      case ALC_EVENT_TYPE_DEVICE_REMOVED_SOFT:
      case AL_EVENT_TYPE_DISCONNECTED_SOFT:
        break;
      default:
        break;
      }
    /*
    auto s    = alcReopenDeviceSOFT(data.dev->dev, nullptr, nullptr);
    auto name = alcGetString(data.dev->dev, ALC_ALL_DEVICES_SPECIFIER);
    Log::d("reopen sound device: ", name, " ret: ", (s ? "true" : "false"));
    */
    }, data.get());
  alcSetThreadContext(nullptr);
  }

  alDistanceModelDirect(data->context, AL_LINEAR_DISTANCE);
  alDisableDirect(data->context, AL_STOP_SOURCES_ON_DISCONNECT_SOFT);
  // TODO: api
  alListenerfDirect(data->context, AL_METERS_PER_UNIT, 100.f);
  process();
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
  alListenerfvDirect(data->context, AL_POSITION, xyz);
  }

void SoundDevice::setListenerPosition(float x, float y, float z) {
  float xyz[] = {x,y,z};
  alListenerfvDirect(data->context, AL_POSITION, xyz);
  }

void SoundDevice::setListenerDirection(const Vec3& f, const Vec3& up) {
  setListenerDirection(f.x,f.y,f.z, up.x,up.y,up.z);
  }

void SoundDevice::setListenerDirection(float dx,float dy,float dz, float ux,float uy,float uz) {
  float ori[6] = {dx,dy,dz, ux,uy,uz};
  alListenerfvDirect(data->context, AL_ORIENTATION, ori);
  }

void SoundDevice::setGlobalVolume(float v) {
  alListenerfDirect(data->context, AL_GAIN, v);
  }

void* SoundDevice::context() {
  return data->context;
  }

#if 0
void* SoundDevice::bufferContext() {
  auto& d = PhysicalDeviceList::inst().buf;
  return d.ctx;
  }

void* SoundDevice::bufferContextSt() {
  auto& d = PhysicalDeviceList::inst().buf;
  return d.ctx;
  }
#endif

std::unique_lock<std::mutex> SoundDevice::globalLock() {
  static std::mutex gil;
  std::unique_lock<std::mutex> g(gil);
  return g;
  }

#endif
