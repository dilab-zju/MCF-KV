//
// Created by recross on 2022/9/30.
//

#ifndef SB_KV_HASHUTIL_H
#define SB_KV_HASHUTIL_H
#include <random>

namespace leveldb {

class TwoIndependentMultiplyShift {
public:
  TwoIndependentMultiplyShift() {
    ::std::random_device random;
    for (auto v : {&multiply_, &add_}) {
      *v = random();
      for (int i = 1; i <= 4; ++i) {
        *v = *v << 32;
        *v |= random();
      }
    }
  }
  uint64_t operator()(uint64_t key) const {
    return (add_ + multiply_ * static_cast<decltype(multiply_)>(key)) >> 64;
  }

private:
  unsigned __int128 multiply_, add_;
};

} // namespace leveldb

#endif // SB_KV_HASHUTIL_H
