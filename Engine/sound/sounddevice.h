#pragma once

#include <memory>

namespace Tempest {

class Sound;
class IDevice;

class SoundDevice final {
  public:
    SoundDevice ();
    SoundDevice (const SoundDevice&) = delete;
    ~SoundDevice();

    SoundDevice& operator = ( const SoundDevice& s) = delete;

    Sound load(const char* fname);
    Sound load(Tempest::IDevice& d);

    void process();
    void suspend();

  private:
    struct Data;
    void* device();
    void  setDevice(void *newDevice);

    std::unique_ptr<Data> data;
  };

}
