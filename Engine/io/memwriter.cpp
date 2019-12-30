#include "memwriter.h"

#include <cstring>

using namespace Tempest;

MemWriter::MemWriter(std::vector<uint8_t> &vec)
  :vec(vec){
  }

size_t MemWriter::write(const void *val, size_t sz) {
  if(sz==0)
    return 0;
  size_t prev=vec.size();
  vec.resize(prev+sz);
  std::memcpy(&vec[prev],val,sz);
  return sz;
  }
