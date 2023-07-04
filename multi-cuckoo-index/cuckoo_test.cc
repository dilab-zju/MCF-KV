//
// Created by recross on 2022/10/10.
//
#include "cuckoo.h"
#include <vector>
#include "util/testharness.h"
#include "util/testutil.h"
#include "util/perf_log.h"


namespace leveldb{

class CuckooFilterTest { };


//TEST(CuckooFilterTest, InsertAndCheckContain){
//    size_t numkeys = 1000000;
//    CuckooFilter* test_cuckoo = new CuckooFilter(numkeys,11111);
//    Status s;
//    uint64_t k;
//    uint32_t fno;
//    uint64_t start_us = benchmark::NowMicros();
//    size_t i = 0;
//    for (; i <numkeys ; ++i) {
//        k = i;
//        fno = i % (260000);
//        s = test_cuckoo->Add(k,fno);
//        if (!s.ok()){
//            std::cout<<k<<"\n";
//            break;
//        }
//    }
//    std::cout<<"insert " << i << "keys\n";
//    std::cout<<"num_kicks:" << test_cuckoo->NumKicks()<<"\n";
//    uint64_t end_us = benchmark::NowMicros();
//    std::cout<<"Put avg latency:"<< (double )(end_us - start_us) / double (i)<<"us\n";
//
//    start_us = benchmark::NowMicros();
//    size_t j = 0;
//    for (; j < i; ++j) {
//        k = j;
//        uint32_t res;
//        if (!test_cuckoo->Contain(k,res)){
//            std::cout<<"failed to found key " << k <<"\n";
//            assert(0);
//        }
//
//        // here could be hash collision, skip this first.
//        if (res != k % 260000){
//            // here could be hash collision, skip this first.
//            std::cout<<"fno not match! " << k % 260000 << "Expected! " << res <<"Got!\n";
//            assert(0);
//        }
//    }
//    end_us = benchmark::NowMicros();
//    std::cout<<"Get avg latency:"<< (double )(end_us - start_us) / double (i)<<"us\n";
//}

}


int main(int argc, char** argv) {
    return leveldb::test::RunAllTests();
}