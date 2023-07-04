//
// Created by recross on 2022/10/1.
//

#ifndef SB_KV_NVMSINGLETABLE_H
#define SB_KV_NVMSINGLETABLE_H

#include <cstdint>
#include <memory>
#include <cassert>
#include <vector>
#include <cstring>
#include <iostream>
#include <libpmem.h>
#include "leveldb/status.h"

#define FP_MASK64 (0xffffffff00000000)
#define FP_MASK32 (0xfffc0000)
#define FN_MASK32 (0x0003ffff)
#define VALID_MASK64 (0x0000000080000000)
#define COLLISION(s1,s2) (((s1)&FP_MASK64) == ((s2)&FP_MASK64))
namespace leveldb {
static const char nvm_dir[] = "/media/pmem1.1/Cuckoo/";
class NvmSingleTable {
public:
    explicit NvmSingleTable(const size_t num, const uint32_t id, uint32_t local_id) : num_buckets_(num), id_(id) {
        size_t size = (num_buckets_ + kPaddingBuckets_) * kBytesPerBucket_;
        int is_pmem = 0;
        size_t mapped_len;
//        buckets_ = (Bucket*) new char[size];
        buckets_ = (Bucket*)(pmem_map_file((nvm_dir + prefix_ + std::to_string(id_) + "." + std::to_string(local_id)).c_str() ,size,PMEM_FILE_CREATE,0666,
                                           &mapped_len, &is_pmem));
        assert(buckets_ != nullptr);
        assert(mapped_len >= size);
        assert(is_pmem);
        mapped_len_ = mapped_len;
        is_pmem_ = is_pmem;
        if (is_pmem_) {
            pmem_memset(buckets_,0,mapped_len,0);
        }else {
            memset(buckets_, 0, size);
        }
    }

    ~NvmSingleTable() {
        //TODO(zouhongjia): free the nvm space allocated
        if (is_pmem_) {
            pmem_unmap(buckets_, mapped_len_);
        }else{
            delete buckets_;
        }
    }

    uint32_t NumBuckets() const {
        return num_buckets_;
    }

    uint64_t NumSlots() const {
        return num_buckets_ * kSlotPerBucket_;
    }

    inline Status InsertTagToBucketAndCheckDup64(const size_t i, const uint64_t slot,
                                                 const bool kickout, uint64_t& oldslot) {
        for (int j = 0; j < kSlotPerBucket_; ++j) {
            uint64_t slotInBucket = ReadSlot64(i, j);
            if (slotInBucket == 0){
                WriteToSlot64(i, j, slot);
                return Status::OK();
            }else if(COLLISION(slotInBucket,slot)){
                return Status::AlreadyExist("hash collision happened");
            }
        }

        if (kickout) {
            size_t r = rand() % kSlotPerBucket_;
            oldslot = ReadSlot64(i, r);
            // Executes cuckoo process, i.e. kick out
            WriteToSlot64(i, r, slot);
        }

        return Status::NotSupported("not enough space");
    }

    inline bool InsertTagToBucket64(const size_t i, const uint64_t slot,
                                    const bool kickout, uint64_t &oldslot) {
        for (int j = 0; j < kSlotPerBucket_; ++j) {
            if (ReadSlot64(i,j) == 0){
                //empty slot
                WriteToSlot64(i,j,slot);
                return true;
            }
        }

        if (kickout) {
            size_t r = rand() % kSlotPerBucket_;
            oldslot = ReadSlot64(i, r);
            // Executes cuckoo process, i.e. kick out
            WriteToSlot64(i, r, slot);
        }
        return false;
    }

    inline bool InsertTagToBucket32(const size_t i, const uint32_t slot,
                                    const bool kickout, uint32_t &oldslot) {
        for (int j = 0; j < kSlotPerBucket_; ++j) {
            if (ReadSlot32(i,j) == 0){
                //empty slot
                WriteToSlot32(i,j,slot);
                return true;
            }
        }

        if (kickout) {
            size_t r = rand() % kSlotPerBucket_;
            oldslot = ReadSlot32(i, r);
            // Executes cuckoo process, i.e. kick out
            WriteToSlot32(i, r, slot);
        }
        return false;
    }

    // `slot` only contains fingerprint part, which means its last 32 bits is zero
    inline bool FindSlotInBuckets64(const size_t i1, const size_t i2,
                                    const uint64_t slot,uint32_t& vec) const{
        for (int i = 0; i < kSlotPerBucket_; ++i) {
            uint64_t res = ReadSlot64Acquire(i1,i);
            if((res & FP_MASK64) == slot ) {
                vec = (res & ~FP_MASK64);
                return true;
            }
        }
        for (int i = 0; i < kSlotPerBucket_; ++i) {
            uint64_t res = ReadSlot64Acquire(i2,i);
            if((res & FP_MASK64) == slot) {
                vec = (res & ~FP_MASK64);
                return true;
            }
        }
        return false;
    }
    inline bool FindSlotInBuckets32(const size_t i1, const size_t i2,
                                    const uint32_t slot, uint32_t& fno) {
        for (int i = 0; i < kSlotPerBucket_; ++i) {
            uint32_t res = ReadSlot32(i1,i);
            if((res & FP_MASK32) == slot) {
                fno = ((res << 14) >> 14);
                return true;
            }
        }


        for (int i = 0; i < kSlotPerBucket_; ++i) {
            uint32_t res = ReadSlot32(i2,i);
            if((res & FP_MASK32) == slot) {
                fno = ((res << 14) >> 14);
                return true;
            }
        }

        return false;

    }

    inline bool FindSlotInBuckets32(const size_t i1, const size_t i2,
                                    const uint32_t slot) {
        for (int i = 0; i < kSlotPerBucket_; ++i) {
            uint32_t res = ReadSlot32(i1,i);
            if((res & FP_MASK32) == slot) {
                return true;
            }
        }


        for (int i = 0; i < kSlotPerBucket_; ++i) {
            uint32_t res = ReadSlot32(i2,i);
            if((res & FP_MASK32) == slot) {
                return true;
            }
        }

        return false;

    }

    inline bool FindSlotInBuckets64(const size_t i1, const size_t i2,
                                    const uint64_t slot) {
        for (int i = 0; i < kSlotPerBucket_; ++i) {
            uint64_t res = ReadSlot64(i1,i);
            if((res & FP_MASK64) == slot) {
                return true;
            }
        }
        for (int i = 0; i < kSlotPerBucket_; ++i) {
            uint64_t res = ReadSlot64(i2,i);
            if((res & FP_MASK64) == slot) {
                return true;
            }
        }
        return false;
    }
    inline bool FindSlotInBucketAndUpdate(const size_t i1, const size_t i2,
                                          const uint64_t fp, const uint32_t fn){
        for (int i = 0; i < kSlotPerBucket_; ++i) {
            uint64_t res = ReadSlot64(i1,i);
            if((res & FP_MASK64) == fp) {
                res = fp | fn;
                WriteToSlot64(i1, i, res);
                return true;
            }
        }
        for (int i = 0; i < kSlotPerBucket_; ++i) {
            uint64_t res = ReadSlot64(i2,i);
            if((res & FP_MASK64) == fp) {
                res = fp | fn;
                WriteToSlot64(i2, i, res);
                return true;
            }
        }
        return false;
    }

    inline bool FindSlotInBucketAndUpdate32(const size_t i1, const size_t i2,
                                          const uint32_t slot){
        for (int i = 0; i < kSlotPerBucket_; ++i) {
            uint32_t res = ReadSlot32(i1,i);
            if ((res & FP_MASK32) == (slot & FP_MASK32)){
                WriteToSlot32(i1, i, slot);
                return true;
            }
        }

        for (int i = 0; i < kSlotPerBucket_; ++i) {
            uint32_t res = ReadSlot32(i1,i);
            if ((res & FP_MASK32) == (slot & FP_MASK32)){
                WriteToSlot32(i2, i, slot);
                return true;
            }
        }
        return false;
    }

    inline bool DeleteFromBucket64(const size_t i, const uint64_t slot){
        for (int j = 0; j < kSlotPerBucket_; ++j) {
            if ((ReadSlot64(i,j)) == slot){
                WriteToSlot64(i,j,0);
                return true;
            }
        }
        return false;
    }


    inline uint64_t Slot(const size_t i, const size_t j){
        return ReadSlot64(i,j);
    }

    inline void WriteSlot(const size_t i, const size_t j, const uint64_t slot) {
        WriteToSlot64(i,j, slot);
    }

    // In Resize, we allocate bigger space for that hash table, moves the data to the new
    // hash table and delete the original one.
    // When doing this, we need a RWMutex to protect the old hash table.
    void Resize() {
        //TODO(zouhongjia): impl later
    }

private:
    static const size_t kSlotPerBucket_ = 4;     // 4 buckets per slot
    static const size_t kBitsPerSlot_ = 32 + 32; // 64 bits per slot
    static const size_t kBytesPerBucket_ =       // 16 bytes per bucket
            (kBitsPerSlot_ * kSlotPerBucket_ + 7) >> 3;
    static const uint64_t kSlotMask =0xffffffffffffffff;
    static const size_t kPaddingBuckets_ =
            ((((kBytesPerBucket_ + 7) >> 3) << 3) - 1) / kBytesPerBucket_;
    const std::string prefix_ = "sht";
    struct Bucket {
        std::atomic<void*> bits_[kSlotPerBucket_];
//        char bits_[kBytesPerBucket_];
    }__attribute__((__packed__));

    Bucket* buckets_;
    size_t num_buckets_;
    size_t id_;
    size_t mapped_len_;
    int is_pmem_;

    inline uint32_t ReadSlot32(const size_t i, const size_t j) const {

//        const char* p = buckets_[i].bits_;
//        uint32_t slot = ((uint32_t*)p)[j];
//        return slot & 0xffffffff;
        return 0;
    }
    inline uint64_t ReadSlot64Acquire(const size_t i, const size_t j) const
    {
        return (uint64_t) (buckets_[i].bits_[j].load(std::memory_order_acquire));
    }

    inline uint64_t ReadSlot64(const size_t i, const size_t j) const {
//        const void* p = (buckets_[i].bits_)[j].load(std::memory_order_consume);
//        const char* p = buckets_[i].bits_;
//        uint64_t slot;
        // jump to no.j slot
//        slot = *((uint64_t *)p);
        return (uint64_t) (buckets_[i].bits_[j].load());
//        return slot;
    }

    inline void WriteToSlot64(const size_t i, const size_t j, const uint64_t t) {

        buckets_[i].bits_[j].store((void *) t, std::memory_order_release);
//        if (is_pmem_){
            pmem_persist(&buckets_[i].bits_[j], 8);
//        }
    }

    inline void WriteToSlot32(const size_t i, const size_t j, const uint32_t t) {
//        char* p = buckets_[i].bits_;
//        uint32_t slot = t & 0xffffffff;
//        ((uint32_t*)p)[j] = slot;
//        pmem_persist(p+4*j,4);
    }

    inline bool IsValid(const uint64_t slot){
        return (slot & VALID_MASK64);
    }

};
}

#endif //SB_KV_NVMSINGLETABLE_H
