//
// Created by recross on 2022/10/1.
//

#ifndef SB_KV_CUCKOO_H
#define SB_KV_CUCKOO_H

#include <atomic>
#include "nvmsingletable.h"
#include "util/hashutil.h"
#include "leveldb/status.h"
#include "util/perf_log.h"
#include "util/rwmutex.h"

#define VALID_MASK64 (0x0000000080000000)
namespace leveldb{

inline uint64_t upperpower2(uint64_t x) {
    x--;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    x |= x >> 32;
    x++;
    return x;
}

static const size_t kMaxCuckoo = 500;
static const double maximum_frac = 0.96;  // Why 0.96? not understand

class CuckooFilter{
public:
    explicit CuckooFilter(const size_t max_num_keys, size_t id);
    ~CuckooFilter();

    const char* Name() const;

    Status Add(uint64_t key,uint32_t file_no);

    bool Contain(const uint64_t key, uint32_t& res);

    bool Contain(const uint64_t key);

    // Marks a key with given file_no as "invalid" i.e. sets the valid bit to zero
    Status MarkInvalid(const uint64_t key, uint32_t file_no);

    Status Update(const uint64_t key, uint32_t file_no);
    //Returns the current load factor of filter.
    double GetLoadFactor() const;

    bool NeedRehash() const;

    //resize the filter to enable more records.
    Status Rehash();

    uint64_t NumKicks() const;

    bool IsRehashing();

    bool IsDeleting();

    static void* ThreadWrapper(void* ptr);

    void SetRehash(bool flag);


private:
    NvmSingleTable* table_;
    NvmSingleTable* alt_table_ = nullptr; // used for resize, nullptr by default
    size_t num_items_ = 0;       // the number of records stored in filter
    size_t max_keys_;                     // if the number of items stored in this filter is
                                          // greater than max_kyes, this filter would be considered unsafe
    uint64_t num_kicks_ = 0;
    size_t local_id_;
    const size_t global_id_;
    size_t bits_per_item_ = 14;                // the bits we encode each item into, default 14 bits
    TwoIndependentMultiplyShift hasher_;
    std::atomic<bool> is_rehashing_;      // boolean flag to show if this filter is rehashing
    std::atomic<bool> is_deleting_;
    readWriteMutex mu_;

    double maximal_load_factor_;


    inline size_t IndexHash(uint32_t hashv, NvmSingleTable* table) const;


    inline uint64_t SlotHash64(uint64_t hashv) const;

    inline uint32_t SlotHash32(uint32_t hashv) const;

    inline size_t AltIndex(const size_t index, const uint32_t slot, NvmSingleTable* table) const;

    inline void GenerateIndexSlotHash(const uint64_t item, size_t* index,
                                      uint32_t * slot) const;

    inline bool IfValid(const uint64_t slot) const;

    Status AddImpl(const size_t i, const uint64_t slot, NvmSingleTable* table);

    // we move data from original table to alt table
    void TransferData();
};

}

#endif //SB_KV_CUCKOO_H
