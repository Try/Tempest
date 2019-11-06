#include "sound.h"

#include <Tempest/IDevice>
#include <Tempest/MemReader>
#include <Tempest/File>
#include <Tempest/Except>

#include <vector>
#include <cstring>

#include <AL/alc.h>
#include <AL/al.h>

using namespace Tempest;

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

Sound::Data::~Data() {
  alDelBuffer(reinterpret_cast<ALbuffer*>(buffer));
  }

uint64_t Sound::Data::timeLength() const {
  auto buf = reinterpret_cast<ALbuffer*>(buffer);
  int size=0,fr=0,bits=0,channels=0;
  if(alGetBufferiCt(buf, AL_SIZE,      &size)    !=AL_NO_ERROR ||
     alGetBufferiCt(buf, AL_BITS,      &bits)    !=AL_NO_ERROR ||
     alGetBufferiCt(buf, AL_CHANNELS,  &channels)!=AL_NO_ERROR ||
     alGetBufferiCt(buf, AL_FREQUENCY, &fr)      !=AL_NO_ERROR) {
    throw std::system_error(Tempest::SoundErrc::NoDevice);
    }

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

bool Sound::isEmpty() const {
  return data==nullptr;
  }

uint64_t Sound::timeLength() const {
  if(data)
    return data->timeLength();
  return 0;
  }

void Sound::implLoad(IDevice &f) {
  auto& mem = f;

  WAVEHeader header={};
  FmtChunk   fmt={};
  size_t     dataSize=0;
  std::unique_ptr<char[]> data = readWAVFull(mem,header,fmt,dataSize);

  int format=0;
  if(data) {
    switch(fmt.bitsPerSample) {
      case 4:
        decodeAdPcm(fmt,reinterpret_cast<uint8_t*>(data.get()),dataSize,uint32_t(-1));
        return;
      case 8:
        format = (fmt.channels==1) ? AL_FORMAT_MONO8  : AL_FORMAT_STEREO8;
        break;
      case 16:
        format = (fmt.channels==1) ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16;
        break;
      default:
        return;
      }

    upload(data.get(),format,dataSize,fmt.samplesPerSec);
    }
  }

std::unique_ptr<char[]> Sound::readWAVFull(IDevice &f, WAVEHeader& header, FmtChunk& fmt, size_t& dataSize) {
  std::unique_ptr<char[]> buffer;

  size_t res = f.read(&header,sizeof(WAVEHeader));
  if(res!=sizeof(WAVEHeader))
    return nullptr;

  if(std::memcmp("RIFF",header.riff,4)!=0 ||
     std::memcmp("WAVE",header.wave,4)!=0)
    return nullptr;

  while(true){
    Header head={};
    if(f.read(&head,sizeof(head))!=sizeof(head))
      break;

    if(head.is("data")){
      buffer.reset(new char[head.size]);
      if(f.read(buffer.get(),head.size)!=head.size){
        buffer.reset();
        return nullptr;
        }
      dataSize = head.size;
      }
    else if(head.is("fmt ")){
      size_t sz=std::min<size_t>(head.size,sizeof(fmt));
      if(f.read(&fmt,sz)!=sz)
        return nullptr;
      f.seek(head.size-sz);
      }
    else if(f.seek(head.size)!=head.size)
      return nullptr;

    if(head.size%2!=0 && f.seek(1)!=1)
      return nullptr;
    }
  return buffer;
  }

void Sound::upload(char* bytes, int format, size_t size, size_t rate) {
  auto b = alNewBuffer();
  if(!b)
    throw std::bad_alloc();

  if(!alBufferDataCt(nullptr, b, format, bytes, int(size), int(rate))) {
    alDelBuffer(b);
    throw std::bad_alloc();
    }

  auto d = std::make_shared<Data>();
  d->buffer=b;
  data = d;
  }

void Sound::decodeAdPcm(const FmtChunk& fmt,const uint8_t* src,uint32_t dataSize,uint32_t maxSamples) {
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

  upload(reinterpret_cast<char*>(dest.data()),AL_FORMAT_MONO16,dest.size(),fmt.samplesPerSec);
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

  size_t chunks = inbufsize/(channels*4);
  samples += chunks*8;

  while(chunks--){
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
