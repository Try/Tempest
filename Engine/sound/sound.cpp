#include "sound.h"

#include <Tempest/IDevice>
#include <Tempest/MemReader>
#include <vector>
#include <cstring>

#include <AL/al.h>
#include <AL/alc.h>

using namespace Tempest;

struct Sound::BasicWAVEHeader final {
  char  riff[4];//'RIFF'
  unsigned int riffSize;
  char  wave[4];//'WAVE'
  char  fmt[4];//'fmt '
  unsigned int fmtSize;
  unsigned short format;
  unsigned short channels;
  unsigned int samplesPerSec;
  unsigned int bytesPerSec;
  unsigned short blockAlign;
  unsigned short bitsPerSample;
  char  data[4];//'data'
  unsigned int dataSize;
  };

Sound::Sound(IDevice& f) {
  std::vector<char> buf;

  {
    char b[2048];
    size_t sz = f.read(b,sizeof(b));
    while( sz>0 ){
      buf.insert( buf.end(), b, b+sz );
      sz = f.read(b,sizeof(b));
      }
  }

  Tempest::MemReader mem(buf.data(), buf.size());

  BasicWAVEHeader header;
  std::unique_ptr<char> data = readWAVFull(mem,header);

  if(data) {
    switch(header.bitsPerSample) {
      case 8:
        format = (header.channels == 1) ? AL_FORMAT_MONO8  : AL_FORMAT_STEREO8;
        break;
      case 16:
        format = (header.channels == 1) ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16;
        break;
      default:
        data = nullptr;
        return;
      }

    upload(data.get(),header.dataSize,header.samplesPerSec);
    } else {
    /*
    OggVorbis_File vf;

    vorbis_info* vi = 0;

    Buf input;
    int64_t uiPCMSamples = 0;
    data = convertOggToPCM( vf, &input, buf.size(), &buf[0], vi, uiPCMSamples );

    if( vi->channels == 1 )
      format = AL_FORMAT_MONO16; else
      format = AL_FORMAT_STEREO16;

    upload( data, uiPCMSamples * vi->channels * sizeof(short), vi->rate );
    ov_clear(&vf);
    */
    }
  }

Sound::Sound(Sound &&s)
  :source(s.source),buffer(s.buffer),format(s.format){
  s.source = 0;
  s.buffer = 0;
  s.format = 0;
  }

Sound::~Sound() {
  if(source)
    alDeleteSources(1, &source);
  if(buffer)
    alDeleteBuffers(1, &buffer);
  }

Sound &Sound::operator =(Sound &&s) {
  std::swap(source, s.source);
  std::swap(buffer, s.buffer);
  std::swap(format, s.format);
  return *this;
  }

void Sound::play() {
  if(source==0)
    return;
  alSourcePlay(source);
  }

void Sound::pause() {
  if(source==0)
    return;
  alSourcePause(source);
  }

std::unique_ptr<char> Sound::readWAVFull(IDevice &f, Sound::BasicWAVEHeader& header) {
  std::unique_ptr<char> buffer;

  size_t res = f.read(reinterpret_cast<char*>(&header),sizeof(BasicWAVEHeader));

  if( res==sizeof(BasicWAVEHeader) ){
    if(!(
      std::memcmp("RIFF",header.riff,4) ||
      std::memcmp("WAVE",header.wave,4) ||
      std::memcmp("fmt ",header.fmt,4)  ||
      std::memcmp("data",header.data,4)
      )){
      buffer.reset(new char[header.dataSize]);

      if(f.read(buffer.get(), header.dataSize )){
        return buffer;
        }
      }
    }
  return nullptr;
  }

void Sound::upload( char* data, size_t size, size_t rate ) {
  alGenBuffers(1, &buffer);
  alBufferData(buffer, format, data, int(size), int(rate));

  alGenSources(1, &source);
  alSourcei(source, AL_BUFFER, int(buffer));
  }
