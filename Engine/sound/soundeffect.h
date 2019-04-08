#pragma once

#include <Tempest/Sound>
#include <cstdint>

namespace Tempest {

class SoundEffect final {
  public:
    SoundEffect()=default;
    SoundEffect(const Sound &src);
    SoundEffect(SoundEffect&& s);
    ~SoundEffect();

    SoundEffect& operator=(SoundEffect&& s);

    void play();
    void pause();

    uint64_t timeLength() const;
    uint64_t currentTime() const;

  private:
    uint32_t source = 0;
    std::shared_ptr<Sound::Data> data;
  };

}
