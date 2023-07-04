//
// Created by recross on 2022/11/17.
//

//#include "multi-cuckoo-index/multi_cuckoo_index.h"
#include "index/ff_btree.h"
#include <vector>
#include "util/testharness.h"
#include "util/perf_log.h"
#include "util/random.h"
#include <queue>
#include <unistd.h>

#define N 3200
#define VAL_SIZE 1024

using namespace leveldb;



#define TOTAL_KEY_NUM (60*1024*1024/32)
leveldb::Random random1(1998);
void GenKeys(std::vector<uint64_t>& keys){
    size_t num_items = TOTAL_KEY_NUM;
    keys.clear();
    keys.resize(num_items);
    uint64_t k = 0;
    for (int i = 0; i < num_items; ++i) {
        k = random1.Next() % num_items;
        keys[i] = k;
    }
}
int main(){
    std::vector<uint64_t> keys;
    GenKeys(keys);
    const double dup_rate = 0.0;
    size_t dup_key_num = dup_rate * TOTAL_KEY_NUM;
    size_t unique_keys = (1.0-dup_rate) *  TOTAL_KEY_NUM;
    vector<uint64_t> diff;
    vector<uint64_t> tmp;
    vector<uint64_t> fp_keys_;
    std::vector<uint64_t> time_elsp;
    uint64_t counter = 0;
    for (int i = 0; i < unique_keys; ++i) {
        tmp.push_back(i);
    }
    Random rand(10);
    Index *index = CreateMCFIndex();
    uint64_t k;
    for (int i = 0; i < unique_keys; ++i) {
        do {
            k = rand.Next() % (unique_keys);
        } while (tmp.at(k) == -1);
        diff.push_back(k);
        fp_keys_.push_back(k + unique_keys);
        tmp.at(k) = -1;
    }
    int rounds = keys.size() / N;
    std::cout << "rounds: "<< rounds << " duplication rate: " <<dup_rate <<" total key num: "<< TOTAL_KEY_NUM << "\n"
                << N << "distinct keys and " << dup_key_num / rounds << "duplicate keys " << " per round\n";
    int cnt = 0;
    std::deque<KeyAndMeta> queue;
    uint64_t sum = 0;
    for (int i = 0; i < rounds; ++i) {
        for (int j = 0; j < N && cnt < keys.size() - 1; ++j) {
            KeyAndMeta keyAndMeta1;
            keyAndMeta1.key = keys[cnt++];
            keyAndMeta1.meta = make_shared<IndexMeta>(0);
            queue.push_back(keyAndMeta1);
        }
        for (int j = 0; j < dup_key_num / rounds; ++j) {
            KeyAndMeta km;
            km.key = diff[rand.Next() % dup_key_num];
            km.meta = make_shared<IndexMeta>(0);
            queue.push_back(km);
        }
        index->AddQueue(queue, nullptr, false);
        queue.clear();
    }
    sleep(10);
    uint64_t start, end, total = 0;
    for (int i = 0; i < diff.size(); ++i) {
        Slice key = Slice(to_string(diff[i]));
        start = benchmark::NowMicros();
        index->Get(key);
        end = benchmark::NowMicros();
        total += (end - start);
    }
    std::cout << "True Positive Get: " << (double )diff.size() / (double )(total) << " ops/microsecond.\n";

    total = 0;
    for (int i = 0; i < fp_keys_.size(); ++i) {
        Slice key = Slice(to_string(fp_keys_[i]));
        start = benchmark::NowMicros();
        index->Get(key);
        end = benchmark::NowMicros();
        total += (end - start);
    }
    std::cout << "False Positive Get: " << (double )fp_keys_.size() / (double )(total) << " ops/microsecond.\n";
    std::cout<<"Total: "<< sum << "micros.\n"
                << (double)sum /  counter << "micros/op.\n";
}