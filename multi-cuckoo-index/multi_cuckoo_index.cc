//
// Created by recross on 2022/10/1.
//

#include "multi_cuckoo_index.h"
#include <iostream>
#include <utility>
#include "util/coding.h"



namespace leveldb {

const size_t max_num_keys = 100000;

MultiCuckooIndex::MultiCuckooIndex(std::string db_name,const size_t num) :db_name_(std::move(db_name)), condVar_(&mutex_), num_cuckoo_filter_(num), load_factor_threshold_(0.85){
    filters_ = new CuckooFilter*[num_cuckoo_filter_];
    for (size_t i = 0; i < num_cuckoo_filter_; ++i) {
        filters_[i] = new CuckooFilter(max_num_keys,i);
    }
    bgstarted_ = false;
}

MultiCuckooIndex::~MultiCuckooIndex() noexcept {
    std::cout<<"MCF destroyed!\n";
}

// for MCI, we first calculate the hash number of the key and then decides
// which CF to go, and then we checks if this cf contains the key(i.e. have
// hash collision), if so, we insert this key into the ff_btree instead
Status MultiCuckooIndex::Insert(const entry_key_t& key, const IndexMeta &meta) {
    if (edit_ != nullptr){
        edit_->AddToRecoveryList(meta.file_number);
    }
    uint32_t cf_no = num_cuckoo_filter_==1? 0 : key % (num_cuckoo_filter_ - 1);
    if (filters_[cf_no]->IsRehashing()){
        return Status::NotSupported("CF is Rehashing now\n");
    }
    if (filters_[cf_no]->Contain(key)){
        if(unlikely(merely_cuckoo_filter)){
            usleep(50);
        }else {
            // insert into ff btree
            IndexMeta *ptr = (IndexMeta *) nvram::pmalloc(sizeof(IndexMeta));
            ptr->file_number = meta.file_number;
            clflush((char *) ptr, sizeof(IndexMeta));
            IndexMeta *old_ptr = (IndexMeta *) tree_.Insert(key, ptr);
            if (old_ptr != nullptr) {
                nvram::pfree(old_ptr);
            }
        }
    }else{
        auto s = filters_[cf_no]->Add(key, meta.file_number);
        if ((!s.ok() || filters_[cf_no]->GetLoadFactor() > load_factor_threshold_) && !filters_[cf_no]->IsRehashing()){
            filters_[cf_no]->SetRehash(true);
            pthread_create(&thread_[cf_no+1],NULL,CuckooFilter::ThreadWrapper,filters_[cf_no]);
            return Status::NotSupported("we need to start rehash now");
        }
    }
    return Status::OK();
}


uint32_t MultiCuckooIndex::Get(const Slice &key) {
    // first read from ff_btree
    uint64_t target_key = fast_atoi(key);
    IndexMeta* result = (IndexMeta*)tree_.Search(target_key);
    if (result != nullptr){
        return result->file_number;
    }

    // failed to read from ff_btree, then we try to find information in CF
    uint32_t cf_no = num_cuckoo_filter_ == 1 ? 0 : target_key % (num_cuckoo_filter_ - 1);
    uint32_t file_no;
    while(filters_[cf_no]->IsDeleting());
    if (filters_[cf_no]->Contain(target_key,file_no)){
        // found in cf
        return file_no;
    }else{
        return -1;
    }
}


void MultiCuckooIndex::Runner() {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-noreturn"
    //bg thread
    while(1){
        mutex_.Lock();
        for (;queue_.empty();) {
            condVar_.Wait();
        }
        if (edit_!= nullptr){
            edit_->AllocateRecoveryList(queue_.size());
        }
        assert(!queue_.empty());
//        std::cout<< queue_.size() <<",";
        uint64_t start = benchmark::NowMicros();
        while(!queue_.empty()) {
            uint64_t key = queue_.front().key;
            std::shared_ptr<IndexMeta> value = queue_.front().meta;
            if (for_update_){
                if (!Update(key, *value).ok()){
                    queue_.push_back((queue_.front()));
                }
            }else{
                if (!Insert(key,*value).ok()){
                    // we move the item from queue head to queue tail if insertion failed
                    queue_.push_back(queue_.front());
                }
            }
            queue_.pop_front();
        }
        if (edit_ != nullptr){
            edit_->Unref();
        }
        uint64_t end = benchmark::NowMicros();
        std::cout << end - start << ",\n";
        assert(queue_.empty());
        mutex_.Unlock();
    }
#pragma clang diagnostic pop
}


// in update, we first check if we can find this key in ff_btree, if so
// we can directly insert into the ff_btree, otherwise we update the key in cf
Status MultiCuckooIndex::Update(entry_key_t key, const IndexMeta &meta) {
    IndexMeta* res = (IndexMeta*)tree_.Search(key);
    if (res != nullptr){
        // found in btree
        IndexMeta* ptr = (IndexMeta*) nvram::pmalloc(sizeof(IndexMeta));
        ptr->file_number = meta.file_number;
        clflush((char*)ptr, sizeof(IndexMeta));
        IndexMeta* old_ptr = (IndexMeta*) tree_.Insert(key,ptr);
        assert(old_ptr!= nullptr);
        assert(old_ptr == res);
        nvram::pfree(old_ptr);
        return Status::OK();
    }else{
        // not found in btree, update cf
        uint32_t cf_no = num_cuckoo_filter_ == 1 ? 0 : key % (num_cuckoo_filter_ - 1);
        if (!filters_[cf_no]->Contain(key)){
            assert(0);
        }
        if (filters_[cf_no]->IsRehashing()){
            return Status::NotSupported("is rehashing");
        }
        if (!filters_[cf_no]->Update(key, meta.file_number).ok()){
            assert(0);
        }
        return Status::OK();
    }
}

void *MultiCuckooIndex::ThreadWrapper(void *ptr) {
    // we start
    reinterpret_cast<MultiCuckooIndex*>(ptr)->Runner();
    return NULL;
}

// Note:: This AddQueue could be called by gc thread, so it would be better to distinguish them
void MultiCuckooIndex::AddQueue(std::deque<KeyAndMeta> &queue, VersionEdit *edit, bool for_update) {
    //TODO(zouhongjia):: Check NULL edit
    mutex_.Lock();
    assert(queue_.empty());
    queue_.swap(queue);
    for_update_ = for_update;
    edit_ = edit;
    if (edit_!= nullptr){
        edit_->Ref();
    }
    //TODO(zouhongjia): Ref the edit
    if (!bgstarted_){
        bgstarted_ = true;
        port::PthreadCall("create thread", pthread_create(&thread_[0], NULL, &MultiCuckooIndex::ThreadWrapper, this));
    }
    condVar_.Signal();
    mutex_.Unlock();
}

void MultiCuckooIndex::Break() {
//    pthread_cancel(thread_[0]);
    for (int i = 0; i < 17; ++i) {
        pthread_cancel(thread_[i]);
    }
}


Iterator *MultiCuckooIndex::NewIterator(const ReadOptions &options, TableCache *table_cache, VersionControl *vcontrol) {
    //TODO(zouhongjia):: not implemented
    return nullptr;
}


}