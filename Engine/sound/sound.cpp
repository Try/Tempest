#if defined(TEMPEST_BUILD_AUDIO)

#include "sound.h"
#include "sounddevice.h"

#include <Tempest/IDevice>
#include <Tempest/MemReader>
#include <Tempest/File>
#include <Tempest/Except>

#include <cassert>
#include <cstring>
#include <algorithm>

#include <AL/alc.h>
#include <AL/al.h>
#include <AL/alext.h>

#include <stdint.h> // used by minivorbis
#include <minivorbis/minivorbis.h>

using namespace Tempest;

static uint8_t channelsCount(int frm) {
  switch (frm) {
    case AL_FORMAT_MONO8:
    case AL_FORMAT_MONO16:
      return 1;
    case AL_FORMAT_STEREO8:
    case AL_FORMAT_STEREO16:
      return 2;
    default:
      assert(0);
    }
  return 0;
  }

static uint8_t bitrate(int frm) {
  switch (frm) {
    case AL_FORMAT_MONO8:
    case AL_FORMAT_STEREO8:
      return 8;
    case AL_FORMAT_MONO16:
    case AL_FORMAT_STEREO16:
      return 16;
    default:
      assert(0);
    }
  return 0;
  }

struct Sound::Header final {
  char     id[4];
  uint32_t size;
  bool     is(const char* n) const { return std::memcmp(id,n,4)==0; }
  };

struct Sound::WAVEHeader final {
  char     riff[4];  //'RIFF'
  uint32_t riffSize;
  char     wave[4];  //'WAVE'
  };

struct Sound::FmtChunk final {
  uint16_t format;
  uint16_t channels;
  uint32_t samplesPerSec;
  uint32_t bytesPerSec;
  uint16_t blockAlign;
  uint16_t bitsPerSample;
  };

const uint16_t Sound::stepTable[89] = {
  7, 8, 9, 10, 11, 12, 13, 14,
  16, 17, 19, 21, 23, 25, 28, 31,
  34, 37, 41, 45, 50, 55, 60, 66,
  73, 80, 88, 97, 107, 118, 130, 143,
  157, 173, 190, 209, 230, 253, 279, 307,
  337, 371, 408, 449, 494, 544, 598, 658,
  724, 796, 876, 963, 1060, 1166, 1282, 1411,
  1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024,
  3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484,
  7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,
  15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794,
  32767
  };

const int32_t Sound::indexTable[] = {
  /* adpcm data size is 4 */
  -1, -1, -1, -1, 2, 4, 6, 8
  };

Sound::Data::~Data() {}

uint64_t Sound::Data::timeLength() const {
  ALint size=0, fr=0, bits=0, channels=0;
  size     = byteSize;
  fr       = frequency;
  bits     = bitrate(format);
  channels = channelsCount(format);

  if(channels<=0 || fr<=0)
    return 0;
  int ssize   = (bits/8)*channels;
  if(ssize<=0)
    return 0;
  int samples = size/ssize;
  return uint64_t(samples*1000)/uint64_t(fr);
  }

Sound::Sound(const char *path) {
  Tempest::RFile f(path);
  implLoad(f);
  }

Sound::Sound(const std::string &path) {
  Tempest::RFile f(path);
  implLoad(f);
  }

Sound::Sound(const char16_t *path) {
  Tempest::RFile f(path);
  implLoad(f);
  }

Sound::Sound(const std::u16string &path) {
  Tempest::RFile f(path);
  implLoad(f);
  }

Sound::Sound(IDevice& f) {
  implLoad(f);
  }

void Sound::initData(const char* bytes, int format, size_t size, size_t rate) {
  auto d = std::make_shared<Data>();
  d->ptr.reset(new char[size]);
  d->byteSize  = uint32_t(size);
  d->frequency = uint32_t(rate);
  d->format    = format;
  std::memcpy(d->ptr.get(), bytes, size);
  data      = d;
  }

void Sound::initData(std::unique_ptr<char[]> buf, int format, size_t size, size_t rate) {
  auto d = std::make_shared<Data>();
  d->ptr = std::move(buf);
  d->byteSize  = uint32_t(size);
  d->frequency = uint32_t(rate);
  d->format    = format;
  data = d;
  }

bool Sound::isEmpty() const {
  return data==nullptr;
  }

uint64_t Sound::timeLength() const {
  if(data)
    return data->timeLength();
  return 0;
  }

void Sound::implLoad(IDevice& fin) {
  char sign[4] = {};
  if(fin.read(sign, 4)!=4)
    throw std::runtime_error("Invalid sound file");

  if(std::memcmp("OggS",sign,4)==0) {
    implLoadOgg(fin);
    return;
    }

  if(std::memcmp("RIFF",sign,4)==0) {
    WAVEHeader header={{'R','I','F','F'}};
    if(fin.read(reinterpret_cast<uint8_t*>(&header)+4,sizeof(WAVEHeader)-4)!=sizeof(WAVEHeader)-4)
      throw std::runtime_error("Invalid sound file");

    if(std::memcmp("WAVE",header.wave,4)!=0)
      throw std::runtime_error("Invalid sound file");

    implLoadWav(fin, header);
    return;
    }
  }

void Sound::implLoadWav(IDevice& fin, const WAVEHeader& header) {
  FmtChunk                fmt = {};
  size_t                  dataSize = 0;
  std::unique_ptr<char[]> buffer;

  while(true){
    Header head={};
    if(fin.read(&head,sizeof(head))!=sizeof(head))
      break;

    if(head.is("data")){
      buffer.reset(new char[head.size]);
      if(fin.read(buffer.get(),head.size)!=head.size){
        throw std::runtime_error("Invalid sound file");
        }
      dataSize = head.size;
      }
    else if(head.is("fmt ")){
      size_t sz=std::min<size_t>(head.size,sizeof(fmt));
      if(fin.read(&fmt,sz)!=sz)
        throw std::runtime_error("Invalid sound file");
      size_t remain = head.size-sz;
      if(fin.seek(remain)!=remain)
        throw std::runtime_error("Invalid sound file");
      }
    else if(fin.seek(head.size)!=head.size)
      throw std::runtime_error("Invalid sound file");

    if(head.size%2!=0 && fin.seek(1)!=1)
      throw std::runtime_error("Invalid sound file");
    }

  int format = 0;
  switch(fmt.bitsPerSample) {
    case 4:
      decodeAdPcm(fmt,reinterpret_cast<uint8_t*>(buffer.get()),uint32_t(dataSize),uint32_t(-1));
      return;
    case 8:
      format = (fmt.channels==1) ? AL_FORMAT_MONO8  : AL_FORMAT_STEREO8;
      initData(std::move(buffer),format,dataSize,fmt.samplesPerSec);
      return;
    case 16:
      format = (fmt.channels==1) ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16;
      initData(std::move(buffer),format,dataSize,fmt.samplesPerSec);
      return;
    }

  throw std::runtime_error("Invalid sound(wav) file bitrate");
  }

void Sound::implLoadOgg(IDevice& fin) {
  ov_callbacks callback = {};
  callback.read_func = [](void *ptr, size_t size, size_t nmemb, void *src) -> size_t {
    auto& f = *reinterpret_cast<IDevice*>(src);
    return f.read(ptr, size*nmemb);
    };

  OggVorbis_File vorbis = {};
  char sgn[] = "OggS";

  std::vector<char> data;
  vorbis_info info = {};
  try {
    if(ov_open_callbacks(&fin, &vorbis, sgn, 4, callback) != 0) {
      throw std::runtime_error("Invalid sound(ogg) file");
      }
    info = *ov_info(&vorbis, -1);

    while(true) {
      int section = 0;
      char buf[16*1024];
      long bytes = ov_read(&vorbis, buf, sizeof(buf), 0, 2, 1, &section);
      if(bytes<0)
        throw std::runtime_error("Invalid sound(ogg) file");
      if(bytes==0)
        break;
      data.insert(data.end(), buf, buf+bytes);
      }
    ov_clear(&vorbis);
    }
  catch(...) {
    ov_clear(&vorbis);
    throw;
    }

  int format = (info.channels==1) ? AL_FORMAT_MONO16  : AL_FORMAT_STEREO16;
  initData(data.data(),format,data.size(),info.rate);
  }

void Sound::decodeAdPcm(const FmtChunk& fmt, const uint8_t* src, uint32_t dataSize, uint32_t maxSamples) {
  if(fmt.blockAlign==0)
    return;

  uint32_t samples_per_block = (fmt.blockAlign-fmt.channels*4)*(fmt.channels^3)+1;
  uint32_t sample_count      = (dataSize/fmt.blockAlign)*samples_per_block;

  if(sample_count>maxSamples)
    sample_count = maxSamples;

  std::vector<uint8_t> dest;
  unsigned             sample = 0;

  while(sample<sample_count) {
    uint32_t block_adpcm_samples = samples_per_block;
    uint32_t block_pcm_samples   = samples_per_block;

    if(block_adpcm_samples>sample_count) {
      block_adpcm_samples = ((sample_count + 6u) & ~7u)+1u;
      block_pcm_samples   = sample_count;
      }

    size_t ofidx = dest.size();
    dest.resize(dest.size()+block_pcm_samples*fmt.channels*2u);

    decodeAdPcmBlock(reinterpret_cast<int16_t*>(&dest[ofidx]), src, fmt.blockAlign, fmt.channels);
    src    += fmt.blockAlign;
    sample += block_pcm_samples;
    }

  initData(reinterpret_cast<char*>(dest.data()),AL_FORMAT_MONO16,dest.size(),fmt.samplesPerSec);
  }

int Sound::decodeAdPcmBlock(int16_t *outbuf, const uint8_t *inbuf, size_t inbufsize, uint16_t channels) {
  int32_t samples = 1;
  int32_t pcmdata[2]={};
  int8_t  index[2]={};

  if(inbufsize<channels * 4 || channels>2)
    return 0;

  for(int ch=0; ch<channels; ch++) {
    *outbuf++ = pcmdata[ch] = int16_t(inbuf [0] | (inbuf [1] << 8));
    index[ch] = inbuf[2];

    if(index[ch]<0 || index[ch]>88 || inbuf[3])     // sanitize the input a little...
      return 0;

    inbufsize -= 4;
    inbuf     += 4;
    }

  int32_t chunks = int32_t(inbufsize/(channels*4));
  samples += chunks*8;

  while(chunks--) {
    for(int ch=0; ch<channels; ++ch) {
      for(int i=0; i<4; ++i) {
        int step = stepTable[index [ch]], delta = step >> 3;

        if (*inbuf & 1) delta += (step >> 2);
        if (*inbuf & 2) delta += (step >> 1);
        if (*inbuf & 4) delta += step;
        if (*inbuf & 8) delta = -delta;

        pcmdata[ch] += delta;
        index  [ch] += indexTable[*inbuf & 0x7];
        index  [ch] = std::min<int8_t>(std::max<int8_t>(index[ch],0),88);
        pcmdata[ch] = std::min(std::max(pcmdata[ch],-32768),32767);
        outbuf[i*2*channels] = int16_t(pcmdata[ch]);

        step  = stepTable[index[ch]];
        delta = step >> 3;

        if (*inbuf & 0x10) delta += (step >> 2);
        if (*inbuf & 0x20) delta += (step >> 1);
        if (*inbuf & 0x40) delta += step;
        if (*inbuf & 0x80) delta = -delta;

        pcmdata[ch] += delta;
        index  [ch] += indexTable[(*inbuf >> 4) & 0x7];
        index  [ch] = std::min<int8_t>(std::max<int8_t>(index[ch],0),88);
        pcmdata[ch] = std::min(std::max(pcmdata[ch],-32768),32767);
        outbuf [(i*2+1)*channels] = int16_t(pcmdata[ch]);

        inbuf++;
        }
      outbuf++;
      }
    outbuf += channels*7;
    }
  return samples;
  }

#endif
