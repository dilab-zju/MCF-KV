//
// Created by recross on 2022/10/1.
//

#include "cuckoo.h"
#include <random>
#include <unistd.h>

namespace leveldb {

CuckooFilter::CuckooFilter(const size_t max_num_keys, size_t id) : global_id_(id), bits_per_item_(14),is_rehashing_(
        false), is_deleting_(false) {
    max_keys_ = max_num_keys;
    //TODO(zouhongjia): Hard code here, modify this later
    size_t assoc = 4;
    size_t num_buckets_ = upperpower2(std::max<uint64_t>(1,max_keys_ / assoc));
    double frac = (double)max_keys_ / num_buckets_ / assoc;

    if (frac > maximum_frac){
        num_buckets_ <<= 1;
    }

    local_id_ = 0;
    table_ = new NvmSingleTable(num_buckets_,id,local_id_++);
    std::default_random_engine e((unsigned (time(0))));
    std::uniform_int_distribution<int> u(-10,10);;
    usleep(1000);
    for (int i = 0; i < id; ++i) {
        e();
    }
    maximal_load_factor_ = 1.00; // for test only
//    maximal_load_factor_ = 0.91 + ((double)(u(e) *  0.004));
    std::cout<<"maximal_load_factor_ of filter."<<id <<": " << maximal_load_factor_ <<std::endl;
}

CuckooFilter::~CuckooFilter() {
    delete table_;
}

const char* CuckooFilter::Name() const {
    return "leveldb.BuiltinNVMCuckooFilter";
}

Status CuckooFilter::Add(uint64_t key, uint32_t file_no) {

    size_t i;
    // we use 64-bit slot, 32bits in the tail is used for key fingerprint
    // last 32 bits is used to store file number
    uint32_t fp;

    GenerateIndexSlotHash(key,&i,&fp);
//    slot = (slot << 18 ) | file_no;
    uint64_t slot = ((uint64_t)fp << 32) | file_no;
    assert((uint32_t)(slot&(~FP_MASK64)) == file_no);
    return AddImpl(i,slot,table_);
}

bool CuckooFilter::Contain(const uint64_t key, uint32_t& res) {
    size_t i1, i2;
    uint32_t slot;
    // the slot generated has no level information,only with lower 8 bits information
    GenerateIndexSlotHash(key,&i1,&slot);
    i2 = AltIndex(i1,slot,table_);
    assert(i1 == AltIndex(i2,slot, table_));
//    mu_.readLock();
    if (table_->FindSlotInBuckets64(i1,i2,(uint64_t)slot << 32,res)){
//        mu_.readUnlock();
        return true;
    }else{
//        mu_.readUnlock();
        return false;
    }
}

bool CuckooFilter::Contain(const uint64_t key) {
    size_t i1, i2;
    uint32_t fp;
    GenerateIndexSlotHash(key,&i1,&fp);
    i2 = AltIndex(i1,fp, table_);
    assert(i1 == AltIndex(i2,fp, table_));
//    mu_.readLock();
    while(is_deleting_.load(std::memory_order_acquire));
    if(table_->FindSlotInBuckets64(i1, i2, (uint64_t)fp << 32 )){
//        mu_.readUnlock();
        return true;
    }else{
//        mu_.readUnlock();
        return false;
    }
}

uint64_t CuckooFilter::NumKicks() const {
    return num_kicks_;
}

Status CuckooFilter::Update(const uint64_t key, uint32_t file_no) {
//    mu_.writeLock();
    size_t i1, i2;
    uint32_t fp;
    GenerateIndexSlotHash(key,&i1,&fp);
    i2 = AltIndex(i1,fp, table_);
    assert(i1 == AltIndex(i2,fp, table_));
    if (table_->FindSlotInBucketAndUpdate(i1, i2, (uint64_t )fp << 32, file_no)){
//        mu_.writeUnlock();
        return Status::OK();
    }else{
//        mu_.writeUnlock();
        std::cout<<"key." << key <<" not found\n";
        return Status::NotFound("key not found\n");
    }
}

bool CuckooFilter::IsRehashing() {
    return is_rehashing_.load(std::memory_order_acquire);
}

bool CuckooFilter::IsDeleting() {
    return is_deleting_.load(std::memory_order_acquire);
}

void CuckooFilter::SetRehash(bool flag) {
    assert(is_rehashing_.load(std::memory_order_acquire) ^ flag);
    is_rehashing_ = flag;
}

void *CuckooFilter::ThreadWrapper(void *ptr) {
    reinterpret_cast<CuckooFilter*>(ptr)->Rehash();
    return NULL;
}

Status CuckooFilter::Rehash() {
    uint64_t start_us = benchmark::NowMicros();
    assert(is_rehashing_);
    max_keys_ <<= 1;
    size_t assoc = 4;
    size_t num_buckets_ = upperpower2(std::max<uint64_t>(1,max_keys_ / assoc));
    double frac = (double)max_keys_ / num_buckets_ / assoc;

    if (frac > maximum_frac){
        num_buckets_ <<= 1;
    }

    // we allocate a new table
    alt_table_ = new NvmSingleTable(num_buckets_,global_id_,local_id_++);
    //TODO(zouhongjia): now we need to transfer the data
//    std::cout<<"start to transferdata!\n";
    TransferData();
//    mu_.writeLock();
    is_deleting_.store(true,std::memory_order_release);
    delete table_;
    table_ = alt_table_;
    alt_table_ = nullptr;
    is_deleting_.store(false);
    is_rehashing_ = false;
//    mu_.writeUnlock();
//    uint64_t end_us = benchmark::NowMicros();
//    std::cout<<"It takes "<< end_us - start_us << " to finish transfer " << num_items_ << "items\n"
//             <<(double)(end_us - start_us) / (double)num_items_ << "us for each item";
    return Status::OK();
}



double CuckooFilter::GetLoadFactor() const {
    return static_cast<double>(num_items_) / static_cast<double>(table_->NumSlots());
}

bool CuckooFilter::NeedRehash() const {
    return static_cast<double>(num_items_) / static_cast<double>(table_->NumSlots())
            >= maximal_load_factor_;
}

size_t CuckooFilter::IndexHash(uint32_t hashv, NvmSingleTable* table) const {
    return hashv & (table->NumBuckets() - 1);
}

uint32_t CuckooFilter::SlotHash32(uint32_t hashv) const {
    uint32_t fp;
    fp = hashv & ((1ULL << bits_per_item_) - 1);
    fp += (fp == 0);
    return fp;
}

uint64_t CuckooFilter::SlotHash64(uint64_t hashv) const {
    uint64_t fp;
    hashv >>= 32;
    hashv += (hashv == 0);
    fp = hashv << 32;
    return fp;
}

size_t CuckooFilter::AltIndex(const size_t index, const uint32_t slot, NvmSingleTable* table) const {
    return IndexHash((uint32_t)(index ^ ((slot >> 16) * 0x5bd1e995)),table);
}


void CuckooFilter::GenerateIndexSlotHash(const uint64_t item, size_t *index, uint32_t *slot) const {
    // Calculate the hash value of item
    const uint64_t hash = hasher_(item);

    // index of item, determines which bucket to go
    *index = IndexHash(hash>>32, table_);

    // get slot via slot hash, it should have a half empty value.
    // *slot = SlotHash32(hash);
    uint32_t fp = hash >> 32;
    fp += (fp == 0);
    *slot = fp;
}

bool CuckooFilter::IfValid(const uint64_t slot) const {
    return true;
}


Status CuckooFilter::AddImpl(const size_t i, const uint64_t slot, NvmSingleTable* table) {
    assert(table!= nullptr);
    size_t cur_index = i;
    uint64_t cur_slot = slot;
    uint64_t old_slot;
//    mu_.writeLock();
    for (uint32_t count= 0;  count < kMaxCuckoo ; count++) {
        bool kickout = count > 0;
        old_slot = 0;
        Status s = table->InsertTagToBucketAndCheckDup64(cur_index, cur_slot, kickout, old_slot);
        if (s.ok()){
            num_items_ ++;
            return Status::OK();
        }else if (s.IsNotSupportedError()){
            assert(kickout == false || old_slot!=0);
            if (kickout){
                num_kicks_++;
                cur_slot = old_slot;
            }

            size_t tmp = AltIndex(cur_index, cur_slot >> 32, table);
            assert(cur_index == AltIndex(tmp, cur_slot >> 32, table));
            cur_index = tmp;
        }else{
            return s;
        }
//        if (table->InsertTagToBucket64(cur_index,cur_slot,kickout,old_slot)){
//            num_items_++;
////            mu_.writeUnlock();
//            return Status::OK();
//        }
//        assert(kickout == false || old_slot!=0);
//        if (kickout){
//            num_kicks_++;
//            cur_slot = old_slot;
//        }
//
//        size_t tmp = AltIndex(cur_index, cur_slot >> 32, table);
//        assert(cur_index == AltIndex(tmp, cur_slot >> 32, table));
//        cur_index = tmp;
    }
//    std::cout<<"max cuckoo\n";
//    mu_.writeUnlock();
    return leveldb::Status::IOError("failed to insert!");
}

void CuckooFilter::TransferData() {
    // transfer all data from table to alt_table.
    uint64_t total = num_items_;
    num_items_ = 0;
    num_kicks_ = 0;
    uint64_t slot;
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < table_->NumBuckets(); ++j) {
            slot = table_->Slot(j,i);
            if (slot!=0) {
                uint32_t index = IndexHash(slot >> 32, alt_table_);
                if (!AddImpl(index,slot,alt_table_).ok()){
//                    std::cout<<"failed to rehash!";
                    assert(0);
                }
                if (num_items_ == total){
//                    std::cout<<"transfer finished!\n";
                    return;
                }
            }
        }
    }
}


}