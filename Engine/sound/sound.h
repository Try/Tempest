#pragma once

#include <Tempest/IDevice>
#include <memory>

namespace Tempest {

class Sound final {
  public:
    Sound(Sound&& s);
    ~Sound();

    Sound& operator = (Sound&& s);

    void play();
    void pause();

  private:
    struct BasicWAVEHeader;

    Sound(Tempest::IDevice& d);
    std::unique_ptr<char> readWAVFull(Tempest::IDevice& d, BasicWAVEHeader &header);
    void                  upload(char *data, size_t size, size_t rate);

    //std::unique_ptr<char> data;
    uint32_t              source = 0;
    uint32_t              buffer = 0;
    int                   format = 0;

  friend class SoundDevice;
  };

}
