//
// Created by recross on 2022/10/20.
//

#ifndef SB_KV_RWMUTEX_H
#define SB_KV_RWMUTEX_H
#include <shared_mutex>
namespace leveldb{
class readWriteMutex {
public:
    readWriteMutex() = default;
    ~readWriteMutex() = default;
    void readLock();
    void readUnlock();
    void writeLock();
    void writeUnlock();

private:
    volatile size_t read_cnt{0};
    volatile size_t write_cnt{0};
    volatile bool inflightwrite{false};
    std::mutex counter_mutex_;
    std::condition_variable cond_w_;
    std::condition_variable cond_r_;
};
}



#endif //SB_KV_RWMUTEX_H
