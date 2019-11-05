#pragma once

#include <Tempest/IDevice>
#include <memory>

namespace Tempest {

class Sound final {
  public:
    Sound()=default;
    Sound(Sound&& s)=default;
    explicit Sound(const char* path);
    explicit Sound(const std::string& path);
    explicit Sound(const char16_t* path);
    explicit Sound(const std::u16string& path);
    explicit Sound(IDevice& input);

    bool     isEmpty() const;
    uint64_t timeLength() const;

  private:
    struct Header;
    struct WAVEHeader;
    struct FmtChunk;

    std::unique_ptr<char[]> readWAVFull(Tempest::IDevice& d, WAVEHeader &header, FmtChunk& fmt, size_t& dataSize);
    void                    upload(char *data, int format, size_t size, size_t rate);
    void                    decodeAdPcm(const FmtChunk& fmt, const uint8_t *src, uint32_t dataSize, uint32_t maxSamples);
    static int              decodeAdPcmBlock(int16_t *outbuf, const uint8_t *inbuf, size_t inbufsize, uint16_t channels);
    void                    implLoad(IDevice& input);

    struct Data {
      ~Data();
      uint32_t buffer=0;
      uint64_t timeLength() const;
      };
    std::shared_ptr<Data> data;

    static const uint16_t stepTable[89];
    static const int32_t  indexTable[];

  friend class SoundDevice;
  friend class SoundEffect;
  };

}
