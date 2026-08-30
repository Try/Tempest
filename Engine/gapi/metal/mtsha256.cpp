#include "mtsha256.h"

#include <algorithm>
#include <cstring>

using namespace Tempest::Detail;

namespace {

constexpr uint32_t k[64] = {
  0x428a2f98u,0x71374491u,0xb5c0fbcfu,0xe9b5dba5u,0x3956c25bu,0x59f111f1u,0x923f82a4u,0xab1c5ed5u,
  0xd807aa98u,0x12835b01u,0x243185beu,0x550c7dc3u,0x72be5d74u,0x80deb1feu,0x9bdc06a7u,0xc19bf174u,
  0xe49b69c1u,0xefbe4786u,0x0fc19dc6u,0x240ca1ccu,0x2de92c6fu,0x4a7484aau,0x5cb0a9dcu,0x76f988dau,
  0x983e5152u,0xa831c66du,0xb00327c8u,0xbf597fc7u,0xc6e00bf3u,0xd5a79147u,0x06ca6351u,0x14292967u,
  0x27b70a85u,0x2e1b2138u,0x4d2c6dfcu,0x53380d13u,0x650a7354u,0x766a0abbu,0x81c2c92eu,0x92722c85u,
  0xa2bfe8a1u,0xa81a664bu,0xc24b8b70u,0xc76c51a3u,0xd192e819u,0xd6990624u,0xf40e3585u,0x106aa070u,
  0x19a4c116u,0x1e376c08u,0x2748774cu,0x34b0bcb5u,0x391c0cb3u,0x4ed8aa4au,0x5b9cca4fu,0x682e6ff3u,
  0x748f82eeu,0x78a5636fu,0x84c87814u,0x8cc70208u,0x90befffau,0xa4506cebu,0xbef9a3f7u,0xc67178f2u,
  };

constexpr uint32_t rotr(uint32_t value, uint32_t count) {
  return (value>>count) | (value<<(32u-count));
  }

uint32_t loadBe(const uint8_t* data) {
  return (uint32_t(data[0])<<24u) | (uint32_t(data[1])<<16u) |
         (uint32_t(data[2])<<8u)  |  uint32_t(data[3]);
  }

}

MtSha256::MtSha256()
  :state{0x6a09e667u,0xbb67ae85u,0x3c6ef372u,0xa54ff53au,
         0x510e527fu,0x9b05688cu,0x1f83d9abu,0x5be0cd19u} {
  }

void MtSha256::update(std::string_view data) {
  update(data.data(),data.size());
  }

void MtSha256::update(const void* ptr, size_t size) {
  if(finalized || size==0)
    return;

  const auto* data = static_cast<const uint8_t*>(ptr);
  byteCount += size;

  if(pendingSize>0) {
    const size_t take = std::min(size_t(64)-pendingSize,size);
    std::memcpy(pending+pendingSize,data,take);
    pendingSize += take;
    data        += take;
    size        -= take;
    if(pendingSize==64) {
      transform(pending);
      pendingSize = 0;
      }
    }

  while(size>=64) {
    transform(data);
    data += 64;
    size -= 64;
    }

  if(size>0) {
    std::memcpy(pending,data,size);
    pendingSize = size;
    }
  }

MtSha256::Digest MtSha256::finalize() {
  if(!finalized) {
    const uint64_t bitCount = byteCount*8u;
    pending[pendingSize++] = 0x80u;
    if(pendingSize>56) {
      std::fill(pending+pendingSize,pending+64,0u);
      transform(pending);
      pendingSize = 0;
      }
    std::fill(pending+pendingSize,pending+56,0u);
    for(size_t i=0; i<8; ++i)
      pending[56+i] = uint8_t(bitCount>>(56u-8u*i));
    transform(pending);
    pendingSize = 0;
    finalized = true;
    }

  Digest ret = {};
  for(size_t i=0; i<8; ++i) {
    ret[i*4+0] = uint8_t(state[i]>>24u);
    ret[i*4+1] = uint8_t(state[i]>>16u);
    ret[i*4+2] = uint8_t(state[i]>>8u);
    ret[i*4+3] = uint8_t(state[i]);
    }
  return ret;
  }

MtSha256::Digest MtSha256::hash(const void* data, size_t size) {
  MtSha256 hash;
  hash.update(data,size);
  return hash.finalize();
  }

MtSha256::Digest MtSha256::hash(std::string_view data) {
  return hash(data.data(),data.size());
  }

void MtSha256::transform(const uint8_t block[64]) {
  uint32_t w[64] = {};
  for(size_t i=0; i<16; ++i)
    w[i] = loadBe(block+i*4);
  for(size_t i=16; i<64; ++i) {
    const uint32_t s0 = rotr(w[i-15],7)^rotr(w[i-15],18)^(w[i-15]>>3u);
    const uint32_t s1 = rotr(w[i-2],17)^rotr(w[i-2],19)^(w[i-2]>>10u);
    w[i] = w[i-16]+s0+w[i-7]+s1;
    }

  uint32_t a=state[0], b=state[1], c=state[2], d=state[3];
  uint32_t e=state[4], f=state[5], g=state[6], h=state[7];
  for(size_t i=0; i<64; ++i) {
    const uint32_t s1    = rotr(e,6)^rotr(e,11)^rotr(e,25);
    const uint32_t choice= (e&f)^((~e)&g);
    const uint32_t temp1 = h+s1+choice+k[i]+w[i];
    const uint32_t s0    = rotr(a,2)^rotr(a,13)^rotr(a,22);
    const uint32_t major = (a&b)^(a&c)^(b&c);
    const uint32_t temp2 = s0+major;
    h=g; g=f; f=e; e=d+temp1;
    d=c; c=b; b=a; a=temp1+temp2;
    }
  state[0]+=a; state[1]+=b; state[2]+=c; state[3]+=d;
  state[4]+=e; state[5]+=f; state[6]+=g; state[7]+=h;
  }
