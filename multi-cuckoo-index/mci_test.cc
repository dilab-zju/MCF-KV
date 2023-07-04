//
// Created by recross on 2022/10/17.
//

#include "multi_cuckoo_index.h"
#include "util/testharness.h"
#include "util/perf_log.h"
#define MAXFILENUM (260000)
namespace leveldb{

class MultiCuckooIndexTest{ };

TEST(MultiCuckooIndexTest, InsertAndGet){
    size_t numkeys = 3000000;
    Index* test_mci = new MultiCuckooIndex("test",2);
    assert(test_mci!= nullptr);
    uint64_t k;
    uint64_t start_us = benchmark::NowMicros();
    Status s;
    size_t i = 0;
    for (; i < numkeys; ++i) {
        k = i;
        IndexMeta meta;
        meta.file_number = i % MAXFILENUM;
        while (!test_mci->Insert(k, meta).ok()){
            usleep(1);
        }
    }
    uint64_t end_us = benchmark::NowMicros();
    std::cout<<"Put avg latency:"<< (double )(end_us - start_us) / double (i)<<"us\n";
    i = 0;
    sleep(2);
    uint64_t total = 0;
    for (; i < numkeys; ++i) {
        k = i;
        Slice key = Slice(to_string(k));
        start_us = benchmark::NowMicros();
        uint32_t fn = test_mci->Get(key);
        end_us = benchmark::NowMicros();
        total += end_us - start_us;
        if (fn == -1){
            cout << "key:" << k << "not found\n";
            assert(0);
        }
        if (fn != k % MAXFILENUM) {
            std::cout<<"key "<<k<<" not match" << k % MAXFILENUM << " Expected but " << fn << "Got\n";
            assert(0);
        }
    }
    std::cout<<"Get avg latency:"<< (double )(total) / double (i)<<"us\n";

}

}

int main() {
    return leveldb::test::RunAllTests();
}