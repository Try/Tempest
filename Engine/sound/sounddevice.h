#pragma once

#include <memory>

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

    void setListenerPosition(float x,float y,float z);
    void setListenerDirection(float dx, float dy, float dz, float ux, float uy, float uz);
    void setGlobalVolume(float v);

  private:
    struct Data;
    struct Device;

    void* context();

    std::unique_ptr<Data> data;

    static std::shared_ptr<Device> device();
    static std::shared_ptr<Device> implDevice();

  friend class SoundEffect;
  };

}
