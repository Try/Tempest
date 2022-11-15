#pragma once

#include <Tempest/Sound>
#include <Tempest/Vec>

#include <cstdint>
#include <array>

namespace Tempest {

class SoundDevice;

class SoundProducer {
  public:
    SoundProducer(uint16_t frequency,uint16_t channels);
    virtual ~SoundProducer()=default;

    virtual void renderSound(int16_t* out,size_t n) = 0;

  private:
    uint16_t frequency = 44100;
    uint16_t channels  = 2;
  friend class SoundEffect;
  };

class SoundEffect final {
  public:
    SoundEffect();
    SoundEffect(SoundEffect&& s);
    ~SoundEffect();

    SoundEffect& operator=(SoundEffect&& s);

    void     play();
    void     pause();

    bool     isEmpty()     const;
    bool     isFinished()  const;
    uint64_t timeLength()  const;
    uint64_t currentTime() const;

    void     setPosition(const Tempest::Vec3& pos);
    void     setPosition(float x,float y,float z);
    void     setMaxDistance(float dist);
    void     setVolume(float val);
    float    volume() const;

    Tempest::Vec3 position() const;
    float    x() const;
    float    y() const;
    float    z() const;

  private:
    SoundEffect(SoundDevice& dev,const Sound &src);
    SoundEffect(SoundDevice& dev,std::unique_ptr<SoundProducer>&& src);

    struct Impl;
    std::unique_ptr<Impl> impl;

  friend class SoundDevice;
  };

}
