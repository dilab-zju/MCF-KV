#include <stdlib.h>
#include "util/coding.h"
#include "leveldb/slice.h"
#include "btree_index.h"
#include "index_iterator.h"
#include "table/format.h"
#include "util/perf_log.h"

namespace leveldb {

BtreeIndex::BtreeIndex() : condvar_(&mutex_) {
  bgstarted_ = false;
}

uint32_t BtreeIndex::Get(const Slice& key) {
  IndexMeta* result = (IndexMeta*)tree_.Search(fast_atoi(key));
  return result->file_number;
}

Status BtreeIndex::Insert(const entry_key_t& key, const IndexMeta& meta) {
//    if (edit_!= nullptr){
//        edit_->AddToRecoveryList(meta.file_number);
//    }
  // check btree if updated
  IndexMeta* ptr = (IndexMeta*) nvram::pmalloc(sizeof(IndexMeta));
  ptr->file_number = meta.file_number;
  clflush((char*)ptr, sizeof(IndexMeta));
  IndexMeta* old_ptr = (IndexMeta*) tree_.Insert(key, ptr);
  if (old_ptr != nullptr) {
//    edit_->DecreaseCount(old_ptr->file_number);
    nvram::pfree(old_ptr);
  }
}

void BtreeIndex::Runner() {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-noreturn"
  for (;;) {
    mutex_.Lock();
    for (;queue_.empty();) {
      condvar_.Wait();
    }
      if (edit_ != nullptr){
          edit_->AllocateRecoveryList(queue_.size());
      }
    assert(!queue_.empty());
      std::cout<< queue_.size() <<",";
      uint64_t start = benchmark::NowMicros();
    for (;!queue_.empty();) {
      uint64_t key = queue_.front().key;
      std::shared_ptr<IndexMeta> value = queue_.front().meta;
      queue_.pop_front();
      Insert(key, *value);
    }
    if (edit_ != nullptr) edit_->Unref();
    assert(queue_.empty());
      uint64_t end = benchmark::NowMicros();
      std::cout << end - start << "\n";
    mutex_.Unlock();
  }
#pragma clang diagnostic pop
}

void* BtreeIndex::ThreadWrapper(void* ptr) {
  reinterpret_cast<BtreeIndex*>(ptr)->Runner();
  return NULL;
}
void BtreeIndex::AddQueue(std::deque<KeyAndMeta>& queue, VersionEdit* edit, bool for_update) {
//  if (edit == nullptr) return;
  mutex_.Lock();
  assert(queue_.size() == 0);
  queue_.swap(queue);
  edit_ = edit;
    if (edit_ != nullptr){
        edit_->Ref();
    }
  if (!bgstarted_) {
    bgstarted_ = true;
    port::PthreadCall("create thread", pthread_create(&thread_, NULL, &BtreeIndex::ThreadWrapper, this));
  }
  condvar_.Signal();
  mutex_.Unlock();
}

Iterator* BtreeIndex::NewIterator(const ReadOptions& options, TableCache* table_cache, VersionControl* vcontrol) {
//  return new IndexIterator(options, tree_.GetIterator(), table_cache, vcontrol);
    return nullptr;
}

FFBtreeIterator* BtreeIndex::BtreeIterator() {
  return tree_.GetIterator();
}

void BtreeIndex::Break() {
  pthread_cancel(thread_);
}


} // namespace leveldb