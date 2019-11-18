#include "memreader.h"

#include <algorithm>
#include <cstring>

using namespace Tempest;

MemReader::MemReader(const char *vec, size_t sz):vec(reinterpret_cast<const uint8_t*>(vec)), sz(sz){
  }

MemReader::MemReader(const uint8_t *vec, size_t sz):vec(vec), sz(sz) {
  }

MemReader::MemReader(const std::vector<char> &vec):MemReader(vec.data(),vec.size()){
  }

MemReader::MemReader(const std::vector<uint8_t> &vec):MemReader(vec.data(),vec.size()){
  }

size_t MemReader::read(void *dest, size_t count) {
  size_t c = std::min(count, sz-pos);

  std::memcpy(dest, vec+pos, c);
  pos+=c;

  return c;
  }

size_t MemReader::size() const {
  return sz;
  }

uint8_t MemReader::peek() {
  if(pos==sz)
    return 0;
  return vec[pos];
  }

size_t MemReader::seek(size_t advance) {
  size_t c = std::min(advance, sz-pos);
  pos += c;
  return c;
  }

size_t MemReader::unget(size_t advance) {
  if(advance>pos){
    size_t ret = advance-(advance-pos);
    pos = 0;
    return ret;
    }
  pos-=advance;
  return advance;
  }
