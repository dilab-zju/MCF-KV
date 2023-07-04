//
// Created by recross on 2022/10/20.
//

#include "rwmutex.h"

namespace leveldb{

void readWriteMutex::readLock() {
    std::unique_lock<std::mutex> lk(counter_mutex_);
    cond_r_.wait(lk, [=]()->bool {return write_cnt == 0;});
    read_cnt++;
}

void readWriteMutex::writeLock() {
    std::unique_lock<std::mutex> lk(counter_mutex_);
    write_cnt++;
    cond_w_.wait(lk, [=]()->bool {return read_cnt == 0 && !inflightwrite;});
    inflightwrite = true;
}

void readWriteMutex::readUnlock() {
    std::unique_lock<std::mutex> lk(counter_mutex_);
    if (--read_cnt == 0 && write_cnt > 0){
        cond_w_.notify_one();
    }
}

void readWriteMutex::writeUnlock() {
    std::unique_lock<std::mutex> lk(counter_mutex_);
    if (--write_cnt == 0){
        cond_r_.notify_all();
    }else{
        cond_w_.notify_one();
    }
    inflightwrite = false;
}



}