//
// Created by recross on 2022/10/1.
//

#ifndef SB_KV_MULTI_CUCKOO_INDEX_H
#define SB_KV_MULTI_CUCKOO_INDEX_H
#define unlikely(x) __builtin_expect(!!(x), 0)

#include "leveldb/index.h"
#include "port/port.h"
#include "cuckoo.h"
#include "leveldb/persistant_pool.h"
#include "util/persist.h"
#include "ff_btreee.h"
#include "db/version_edit.h"
#include "util/rwmutex.h"

namespace leveldb {

const bool merely_cuckoo_filter = false;

class MultiCuckooIndex : public Index {
public:
    MultiCuckooIndex(std::string db_name, const size_t num);

    ~MultiCuckooIndex();

    Status Insert(const entry_key_t& key, const IndexMeta& meta);

    uint32_t Get(const Slice& key);

    void AddQueue(std::deque<KeyAndMeta>& queue, VersionEdit* edit, bool for_update = false);

    Iterator* NewIterator(const ReadOptions& options, TableCache* table_cache, VersionControl* vcontrol);

    void Break();
private:
    void Runner();
    static void* ThreadWrapper(void *ptr);

    Status Update(entry_key_t key, const IndexMeta& meta);

    bool bgstarted_;
    size_t num_cuckoo_filter_;
    CuckooFilter** filters_; // to store non-collision keys
    FFBtree tree_;           // to store collision keys
    const double load_factor_threshold_;
    pthread_t thread_[17];  // thread[0] is used for
    port::Mutex mutex_;
    port::CondVar condVar_;

    std::deque<KeyAndMeta> queue_;
    std::atomic<bool> for_update_;
    std::string db_name_;
    VersionEdit* edit_ = nullptr;
};
}

#endif //SB_KV_MULTI_CUCKOO_INDEX_H
