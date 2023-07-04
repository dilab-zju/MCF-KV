//
// Created by recross on 2022/9/30.
//


#include "leveldb/dynamicfilter.h"
#include "singletable.h"
#include "hashutil.h"
#include "unordered_map"
#include <iostream>

#define fpv(x,s) ((x)>>(s))
#define fpv32(x) ((x)&0xffff0000)
#define slotv(x,s) ((x)<<(s))
#define MASKLAST32IN64 (0x00000000fffffffff)


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
static const size_t fn_size = 16;         // TODO(zouhongjia):this needs to be changed?
static const double maximum_frac = 0.96;  // Why 0.96? not understand

class CuckooFilter : public DynamicFilter{
public:
    explicit CuckooFilter(const size_t max_num_keys){
        size_t assoc = 4;
        size_t num_buckets = upperpower2(std::max<uint64_t>(1,max_num_keys / assoc));
        double frac = (double)max_num_keys / num_buckets / assoc;


        if (frac > maximum_frac){
            num_buckets <<= 1;
        }

        victim_.used = false;
        table_ = new SingleTable(num_buckets);
    }
    ~CuckooFilter() {delete table_;}

    const char* Name() const override {return  "leveldb.BuiltinCuckooFilter";}

    Status Add(uint64_t key,uint32_t file_no) override{
        size_t i;

        // we use 64-bit slot, 32bits in the tail is used for key fingerprint
        // last 32 bits is used to store file number
        uint64_t slot;
        // there is no victim
        GenerateIndexSlotHash(key,&i,&slot);

        //encode file_no into the last 32 bits of the slot
        slot = (slot << 32) | file_no;
        assert((uint32_t)(slot&MASKLAST32IN64) == file_no);
        return AddImpl(i,slot);
    }

    Status Contain(const uint64_t key,std::vector<uint32_t>& vec) override;



private:
    SingleTable * table_;
    size_t num_items_;

    typedef struct {
        size_t index;
        uint32_t slot;
        bool used;
    }VictimCache;

    VictimCache victim_; // victim is used for the last entry that cant be insert into the filter

    TwoIndependentMultiplyShift hasher_;

    inline size_t IndexHash(uint32_t hashv) const{
        return hashv & (table_->NumBuckets() - 1);
    }



    inline uint64_t SlotHash64(uint64_t hashv) const {
        uint64_t fp;
        //we encode hashv to 32 bits
        fp = hashv & ((1ULL << 32) - 1);
        fp += (fp==0);
        return fp;
    }

    inline uint32_t SlotHash32(uint32_t hashv) const{
        uint32_t fp;
        // we encode hashv to 16 bits
        fp = hashv & ((1ULL << 16) -1);
        // fp equals to 1 only when hashv == 0
        fp += (fp == 0);
        return fp;
    }

    inline size_t AltIndex(const size_t index, const uint32_t slot) const{
        return IndexHash((uint32_t)(index ^ ((slot) * 0x5bd1e995)));
    }
    inline void GenerateIndexSlotHash(const uint64_t item, size_t* index,
                                      uint64_t * slot) const{
        // Calculate the hash value of item
        const uint64_t hash = hasher_(item);

        // index of item, determines which bucket to go
        *index = IndexHash(hash>>32); // index is independent of fp

        // get slot via slot hash, it should have a half empty value.
        *slot = SlotHash64(hash);
    }

    Status AddImpl(const size_t i, const uint64_t slot);

};



Status CuckooFilter::AddImpl(const size_t i, const uint64_t slot) {
    size_t cur_index = i;
    uint64_t cur_slot = slot;
    uint64_t old_slot;

    for (uint32_t count= 0;  count < kMaxCuckoo ; count++) {
        bool kickout = count > 0;
        old_slot = 0;
        if (table_->InsertTagToBucket64(cur_index,cur_slot,kickout,old_slot)){
            num_items_++;
            return Status::OK();
        }
        assert(kickout == false || old_slot!=0);
//        assert(cur_slot!=old_slot);
        if (kickout){
            cur_slot = old_slot;
        }
        // remember to clear the low 4bits of slot

        size_t tmp = AltIndex(cur_index, fpv(cur_slot,32));
        assert(cur_index == AltIndex(tmp, fpv(cur_slot,32)));
        cur_index = tmp;
    }

    //TODO(zouhongjia): key has not been inserted yet, resize the filter and rebuild
    //the filter, which may be a performance bottleneck.
    table_->SetFull(cur_index);  // set the bucket at ci as full

    //now we use a dynamic part
    table_->InsertIntoDynamic(cur_index,slot);
    return Status::OK();
}

Status CuckooFilter::Contain(const uint64_t key, std::vector<uint32_t> &vec) {
    size_t i1,i2;
    uint64_t slot;
    // the slot generated has no level information,only with lower 8 bits information
    GenerateIndexSlotHash(key,&i1,&slot);
    i2 = AltIndex(i1,slot);
    assert(i1 == AltIndex(i2,slot));
    if (table_->FindSlotInBucketsV2(i1,i2, slotv(slot,32),vec)){
        return Status::OK();
    }else{
        return leveldb::Status::NotFound("key not found");
    }

}

// to check whether this key is in the filter
//Status CuckooFilter::Contain(const uint64_t key, uint8_t& level1,uint8_t& level2) {
//    bool found = false;
//    size_t i1,i2;
//    uint32_t slot;
//    // the slot generated has no level information,only with lower 8 bits information
//    GenerateUbdexSlotHash(key,&i1,&slot);
//    i2 = AltIndex(i1,slot);
//    assert(i1 == AltIndex(i2,slot));
//
//    found = victim_.used && (slot==(fpv(slot,16))) &&
//            (i1 == victim_.index || i2 == victim_.index);
//    if (found || table_->FindSlotInBuckets(i1,i2,slot<<4,level1,level2)){
//        return leveldb::Status::OK();
//    }else{
//        return leveldb::Status::NotFound("key not found");
//    }
//}
Status CuckooFilter::Delete(const uint64_t key, const uint16_t file_no) {
    size_t i1, i2;
    uint32_t slot;
    GenerateIndexSlotHash(key,&i1,&slot);
    i2 = AltIndex(i1,slot);
    assert(i1 == AltIndex(i2,slot));

    if (table_->DeleteFromBucketV2(i1, slotv(slot,16) | file_no)){
        num_items_--;
        return Status::OK();
    }else if (table_->DeleteFromBucketV2(i2, slotv(slot,16) | file_no)){
        num_items_--;
        return Status::OK();
    }
    return Status::NotFound("key not found");
}


Status CuckooFilter::Delete(const uint64_t key,const uint8_t level) {
    assert(victim_.used == false);
    size_t i1, i2;
    uint32_t slot;
    GenerateIndexSlotHash(key,&i1,&slot);
    i2 = AltIndex(i1,slot);
    assert(i1 == AltIndex(i2,slot));

    if (table_->DeleteFromBucket(i1, slotv(slot,16) | level)){
        num_items_--;
        if (victim_.used){
            victim_.used = false;
            AddImpl(victim_.index,victim_.slot);
        }
        return Status::OK();
    }else if (table_->DeleteFromBucket(i2,slotv(slot,16) | level)){
        num_items_--;
        if (victim_.used){
            victim_.used = false;
            AddImpl(victim_.index,victim_.slot);
        }
        return Status::OK();
    }else if (victim_.used && (victim_.slot>>4) == slot &&
              (i1 == victim_.index || i2 == victim_.index)){
        victim_.used = false;
        return Status::OK();
    }else{
        return Status::NotFound("key not found");
    }
}
DynamicFilter* NewCuckooFilter(const size_t max_num_keys){
    return new CuckooFilter(max_num_keys);
}

} // namespace leveldb