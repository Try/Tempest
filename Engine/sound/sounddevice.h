#pragma once

#include <Tempest/Vec>

#include <memory>
#include <mutex>

namespace Tempest {

class Sound;
class SoundProducer;
class SoundEffect;
class IDevice;

class SoundDevice final {
  public:
    SoundDevice ();
    SoundDevice (const SoundDevice&) = delete;
    ~SoundDevice();

    SoundDevice& operator = ( const SoundDevice& s) = delete;

    SoundEffect load(const char* fname);
    SoundEffect load(Tempest::IDevice& d);
    SoundEffect load(const Sound& snd);
    SoundEffect load(std::unique_ptr<SoundProducer> &&p);

    void process();
    void suspend();

    void setListenerPosition(const Tempest::Vec3& p);
    void setListenerPosition(float x,float y,float z);

    void setListenerDirection(const Tempest::Vec3& forward, const Tempest::Vec3& up);
    void setListenerDirection(float dx, float dy, float dz, float ux, float uy, float uz);

    void setGlobalVolume(float v);

  private:
    struct Data;
    struct Device;

    void* context();

    std::unique_ptr<Data> data;

    static std::unique_lock<std::mutex> globalLock();
    static void                         setThreadContext();

    static std::shared_ptr<Device>      device();
    static std::shared_ptr<Device>      implDevice();

  friend class Sound;
  friend class SoundEffect;
  };

}
