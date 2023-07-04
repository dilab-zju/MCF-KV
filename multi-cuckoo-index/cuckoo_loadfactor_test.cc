//
// Created by recross on 2022/11/11.
//


#include "cuckoo.h"
#include <vector>
#include "util/testharness.h"
#include "util/testutil.h"
#include "util/perf_log.h"
#include <fstream>

namespace leveldb {
    class CuckooLoadfactorTest {
    };

    TEST(CuckooLoadfactorTest, BatchInsert) {
        size_t num_of_cuckoo_ = 100;
        size_t num_keys = 500000 * 20;
        std::vector<double> lf_vec_;
        for (int i = 0; i < num_of_cuckoo_; ++i) {
            CuckooFilter *test_cuckoo = new CuckooFilter(num_keys, 0);
            Status s;
            uint64_t k = 0;
            uint32_t fno = 1;
            while (true) {
                s = test_cuckoo->Add(k++, fno);
                if (!s.ok()) {
                    break;
                }
            }
            lf_vec_.push_back(test_cuckoo->GetLoadFactor());
            delete test_cuckoo;
        }
        double res = 0.0;
        for (int i = 0; i < lf_vec_.size(); ++i) {
            res += lf_vec_[i];
        }
        res /= (double )num_of_cuckoo_;
        std::cout <<" average load factor of " << num_keys <<" Cuckoo filter is: " << res;
    }
}

int main(int argc, char** argv) {
    return leveldb::test::RunAllTests();
}