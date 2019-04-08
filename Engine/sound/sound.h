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
    struct Header;
    struct WAVEHeader;
    struct FmtChunk;

    Sound(Tempest::IDevice& d);
    std::unique_ptr<char> readWAVFull(Tempest::IDevice& d, WAVEHeader &header, FmtChunk& fmt, size_t& dataSize);
    void                  upload(char *data, size_t size, size_t rate);
    void                  decodeAdPcm(const FmtChunk& fmt, const uint8_t *src, uint32_t dataSize, uint32_t maxSamples);
    static int            decodeAdPcmBlock(int16_t *outbuf, const uint8_t *inbuf, size_t inbufsize, uint16_t channels);

    //std::unique_ptr<char> data;
    uint32_t              source = 0;
    uint32_t              buffer = 0;
    int                   format = 0;

    static const uint16_t stepTable[89];
    static const int32_t  indexTable[];
  friend class SoundDevice;
  };

}
