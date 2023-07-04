//
// Created by recross on 2022/11/18.
//

#include "leveldb/filter_policy.h"

#include "util/coding.h"
#include "util/logging.h"
#include "util/testharness.h"
#include "db/dbformat.h"
#include "util/perf_log.h"
#include <vector>
#include <queue>

#define N 64000
#define VAL_SIZE 1024

constexpr size_t nvm_size = 1024*1024*1024;

#define TOTAL_KEY_NUM (50*1024*1024/32)
using namespace leveldb;

int main(){
    const FilterPolicy* policy = NewBloomFilterPolicy(15);
    std::string filter;
    std::vector<std::string> keys_;
    std::vector<std::string> filters_;
    const double dup_rate = 0.0;
    size_t dup_key_num = dup_rate * TOTAL_KEY_NUM;
    size_t unique_keys = (1.0-dup_rate) *  TOTAL_KEY_NUM;
    std::vector<uint64_t> diff;
    std::vector<uint64_t> tmp;
    std::vector<uint64_t> time_elsp;
    uint64_t counter = 0;
    for (int i = 0; i < unique_keys; ++i) {
        tmp.push_back(i);
    }
    Random rand(10);
    uint64_t k;
    for (int i = 0; i < unique_keys; ++i) {
        do {
            k = rand.Next() % (unique_keys);
        } while (tmp.at(k) == -1);
        diff.push_back(k);
        tmp.at(k) = -1;
    }
    int rounds = unique_keys / N;
    std::cout << "rounds: "<< rounds << " duplication rate: " <<dup_rate <<" total key num: "<< TOTAL_KEY_NUM << "\n"
              << N << "distinct keys and " << dup_key_num / rounds << "duplicate keys " << " per round\n";
    int cnt = 0;
    uint64_t sum = 0;
    for (int i = 0; i < rounds; ++i) {
        for (int j = 0; j < N && cnt < diff.size(); ++j) {
            char k1[100];
            snprintf(k1, sizeof(k1), config::key_format, diff[cnt++]);
            Slice key = k1;
            keys_.push_back(key.ToString());
        }
        for (int j = 0; j < dup_key_num / rounds;++j) {
            char k1[100];
            snprintf(k1, sizeof(k1), config::key_format, diff[rand.Next() % dup_key_num]);
            Slice key = k1;
            keys_.push_back(key.ToString());
        }
        std::cout<<keys_.size() << ",";
        uint64_t start = benchmark::NowMicros();
        std::vector<Slice> key_slices;
        for (int j = 0; j < keys_.size(); ++j) {
            key_slices.push_back(Slice(keys_[i]));
        }
        filter.clear();
        policy->CreateFilter(&key_slices[0], static_cast<int>(key_slices.size()),
                              &filter);
        keys_.clear();
        uint64_t end = benchmark::NowMicros();
        std::cout << end - start <<"\n";
    }
}
