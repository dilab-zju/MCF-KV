//
// Created by recross on 2022/9/30.
//

#ifndef SB_KV_SINGLETABLE_H
#define SB_KV_SINGLETABLE_H

#include <cstdint>
#include <memory>
#include <assert.h>
#include <vector>
#include <unordered_map>
#include <cstring>

namespace leveldb{


#define haszero16(x) \
  (((x)-0x0001000100010001ULL) & (~(x)) & 0x8000800080008000ULL)
#define hasvalue16(x, n) (haszero16((x) ^ (0x0001000100010001ULL * (n))))
#define slotindex(x)  ((((x)&0x0000000000008000ULL) >> 15) + (((x)&0x0000000080000000ULL) >> 30) \
                            +(((x)&0x0000800000000000ULL) >> 45) + (((x)&0x8000000000000000ULL >> 60)))

#define BUCKET_MASK (0xfff0fff0fff0fff0)
#define LEVEL_VAL_MASK (0x0000ffff)
#define bit_pos(x)  (1<<((x)%8))
    class SingleTable{
    public:
        explicit SingleTable(const size_t num) : num_buckets_(num){
            buckets_ = new Bucket[num_buckets_ + kPaddingBuckets_];
            memset(buckets_,0,kBytesPerBucket_*(num_buckets_ + kPaddingBuckets_));
            bitmap_ = new char[(num_buckets_ + 7) >> 3];
            memset(bitmap_,0,(num_buckets_ + 7) >> 3);
        }

        ~SingleTable(){
            delete[] buckets_;
        }

        uint32_t NumBuckets() const {
            return num_buckets_;
        }

        inline bool SameSlot(const size_t i, const size_t j) const{
            const char* p1 = buckets_[i].bits_;
            const char* p2 = buckets_[j].bits_;
            return (*((uint64_t*)p1)) == (*((uint64_t*)p2));
        }

        inline uint32_t ReadSlot(char* p, const size_t j) const {
            uint32_t slot;
            p += (j<<2);
            slot = *((uint32_t *)p);
            return slot & kSlotMask;
        }

        inline uint64_t ReadSlot64(const size_t i, const size_t j) const {
            const char* p = buckets_[i].bits_;
            uint64_t slot;
            // jump to no.j slot
            p += (j << 3);
            slot = *((uint64_t *)p);
            return slot & kSlotMask;
        }

        inline uint32_t ReadSlot(const size_t i, const size_t j) const {
            const char* p = buckets_[i].bits_;
            // 32 bits per slot
            uint32_t slot;
            p += (j<<2);
            slot = *((uint32_t *)p);
            return slot & kSlotMask;
        }

        inline void WriteToSlot(char* p, const size_t j, const uint32_t t){
            uint32_t slot = t & kSlotMask;
            ((uint32_t*)p)[j] = slot;
        }

        inline void WriteToSlot64(const size_t i, const size_t j, const uint64_t t) {
            char *p = buckets_[i].bits_;
            uint64_t slot = t & kSlotMask;
            ((uint64_t *)p)[j] = slot;
        }

        inline void WriteToSlot(const size_t i, const size_t j, const uint32_t t){
            char *p = buckets_[i].bits_;
            uint32_t slot = t & kSlotMask;
            // 16bits per slot
            ((uint32_t *)p)[j] = slot;
        }

        inline bool FindSlotInBucketsV2(const size_t i1, const size_t i2,
                                        const uint32_t slot,std::vector<uint32_t>& vec) const{
            char* p = buckets_[i1].bits_;
            for (int i = 0; i < kSlotPerBucket_; ++i) {
                if ((ReadSlot(p,i)&(~LEVEL_VAL_MASK)) == slot){
                    vec.push_back(ReadSlot(p,i)&LEVEL_VAL_MASK);
                }
            }
            if (BucketFull(i1)){
                assert(dynamic_part_.find(i1)!=dynamic_part_.end());
                BucketForDynamic* bucket = dynamic_part_.find(i1)->second;
                while(bucket!= nullptr){
                    p = bucket->bits_;
                    for (int i = 0; i < bucket->index; ++i) {
                        if ((ReadSlot(p,i)&(~LEVEL_VAL_MASK)) == slot){
                            vec.push_back(ReadSlot(p,i)&LEVEL_VAL_MASK);
                        }
                    }
                    bucket = bucket->p;
                }
            }
            p = buckets_[i2].bits_;
            for (int i = 0; i < kSlotPerBucket_; ++i) {
                if ((ReadSlot(p,i)&(~LEVEL_VAL_MASK)) == slot){
                    vec.push_back(ReadSlot(p,i)&LEVEL_VAL_MASK);
                }
            }
            if (BucketFull(i2)){
                assert(dynamic_part_.find(i2)!=dynamic_part_.end());
                BucketForDynamic* bucket = dynamic_part_.find(i2)->second;
                while(bucket!= nullptr){
                    p = bucket->bits_;
                    for (int i = 0; i < bucket->index; ++i) {
                        if ((ReadSlot(p,i)&(~LEVEL_VAL_MASK)) == slot){
                            vec.push_back(ReadSlot(p,i)&LEVEL_VAL_MASK);
                        }
                    }
                    bucket = bucket->p;
                }
            }
            return !vec.empty();
        }

        inline bool FindSlotInBuckets(const size_t i1, const size_t i2,
                                      const uint32_t slot, std::vector<uint16_t>& vec) const{
            for (int j = 0; j < kSlotPerBucket_; ++j) {
                if ((ReadSlot(i1,j)&(~LEVEL_VAL_MASK)) == slot){
                    vec.push_back((ReadSlot(i1,j)&LEVEL_VAL_MASK));
                }
            }
            for (int j = 0; j < kSlotPerBucket_; ++j) {
                if ((ReadSlot(i2,j)&(~LEVEL_VAL_MASK)) == slot){
                    vec.push_back((ReadSlot(i2,j)&LEVEL_VAL_MASK));
                }
            }
            return !vec.empty();
        }

        inline bool FindSlotInBuckets(const size_t i1, const size_t i2,
                                      const uint32_t slot,uint8_t& l1,uint8_t&l2) const{
            const char* p1 = buckets_[i1].bits_;
            const char* p2 = buckets_[i2].bits_;

            uint64_t v1 = *((uint64_t *)p1);
            v1 &= BUCKET_MASK;
            uint64_t v2 = *((uint64_t *)p2);
            v2 &= BUCKET_MASK;
            uint64_t sig_bits1;
            uint64_t sig_bits2;
            size_t index;

            if ((sig_bits1 = hasvalue16(v1,slot)) && (sig_bits2 = hasvalue16(v2,slot))){
                // not that it is possible for the first hashvalue to be false positive
                index =  slotindex(sig_bits1) == 8? 3: slotindex(sig_bits1)>>1;
                assert(index <= 3);
                l1 = (uint8_t)((ReadSlot(i1,index) & LEVEL_VAL_MASK));
                index =  slotindex(sig_bits2) == 8? 3: slotindex(sig_bits1)>>1;
                assert(index <= 3);
                l2 = (uint8_t)((ReadSlot(i2,index) & LEVEL_VAL_MASK));
                return true;
            }else if ((sig_bits1 = hasvalue16(v1,slot)) ){
                index =  slotindex(sig_bits1) == 8? 3: slotindex(sig_bits1)>>1;
                assert(index <= 3);
                l1 = (uint8_t)((ReadSlot(i1,index) & LEVEL_VAL_MASK));
                return true;
            }else if ((sig_bits2 = hasvalue16(v2,slot))){
                index =  slotindex(sig_bits2) == 8? 3: slotindex(sig_bits1)>>1;
                assert(index <= 3);
                l2 = (uint8_t)((ReadSlot(i2,index) & LEVEL_VAL_MASK));
                return true;
            }else return false;
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
                oldslot = ReadSlot(i, r);
                // Executes cuckoo process, i.e. kick out
                WriteToSlot64(i, r, slot);
            }
            return false;
        }

        inline bool InsertTagToBucket(const size_t i, const uint32_t slot,
                                      const bool kickout, uint32_t &oldslot){
            for (int j = 0; j < kSlotPerBucket_; ++j) {
                if (ReadSlot(i,j) == 0){
                    //empty slot
                    WriteToSlot(i,j,slot);
                    return true;
                }
            }
            if (kickout){
                size_t r = rand() % kSlotPerBucket_;
                oldslot = ReadSlot(i,r);
                WriteToSlot(i,r,slot);
            }
            return false;
        }
        inline bool DeleteFromBucketV2(const size_t i, const uint32_t slot){
            //TODO:: de we need to transfer the data in the last Bucket to former one?
            // the data transfer work is necessary and the new bucket list should employ
            // head insert method.
            bool full = BucketFull(i);
            char* p = buckets_[i].bits_;
            for (int j = 0; j < kSlotPerBucket_; ++j) {
                if ((ReadSlot(p,j)) == slot){
                    if (full){
                        WriteToSlot(p,j,transfer(i));
                        if (dynamic_part_[i] == nullptr) SetEmpty(i);
                    }else{
                        WriteToSlot(p,j,0);
                    }
                    return true;
                }
            }
            if (full){
                uint32_t replacement = transfer(i);
                if (replacement == slot) return true;
                BucketForDynamic* bucket = dynamic_part_[i];
                while(bucket!= nullptr){
                    p = bucket->bits_;
                    for (int j = 0; j < bucket->index; ++j) {
                        if ((ReadSlot(p,j)) == slot){
                            WriteToSlot(p,j,replacement);
                            return true;
                        }
                    }
                    bucket = bucket->p;
                }
            }
            return false;
        }

        inline bool DeleteFromBucket(const size_t i, const uint32_t slot){
            for (int j = 0; j < kSlotPerBucket_; ++j) {
                if ((ReadSlot(i,j)) == slot){
                    WriteToSlot(i,j,0);
                    return true;
                }
            }
            return false;
        }

        inline void SetFull(const size_t i){
            bitmap_[i / 8] |= (uint8_t)(bit_pos(i));
        }

        inline void SetEmpty(const size_t i){
            bitmap_[i/8] &= ~(bit_pos(i));
        }

        inline bool BucketFull(const size_t i) const {
            return bitmap_[i/8] & bit_pos(i);
        }


        void InsertIntoDynamic(const size_t i, const uint64_t slot){
            assert(BucketFull(i));
            if (dynamic_part_.find(i) == dynamic_part_.end()){
                //first insert, allocate the memory
                // it should use the same bucket as tha hash table
                dynamic_part_[i] = new BucketForDynamic;
                char* p = dynamic_part_[i]->bits_;
                WriteToSlot(p,0,slot);
                dynamic_part_[i]->index++;
                return;
            }else{
                BucketForDynamic* bucket = dynamic_part_[i];
                char* p = bucket->bits_;
                if (bucket->index != kSlotPerBucket_ * 2){
                    WriteToSlot(p,bucket->index++,slot);
                }else{
                    //this slot is full, allocate new Bucket using head insert
                    auto* NewBucket = new BucketForDynamic;
                    assert(NewBucket!= nullptr);
                    NewBucket->p = dynamic_part_[i];
                    dynamic_part_[i] = NewBucket;
                    p = dynamic_part_[i]->bits_;
                    WriteToSlot(p,NewBucket->index++,slot);
                }
                return;
            }
        }



    private:
        static const size_t kSlotPerBucket_ = 4;
        static const size_t kBitsPerSlot_ = 32 + 32; // 64bits per slot
        static const size_t kBytesPerBucket_ =
                (kBitsPerSlot_ * kSlotPerBucket_ + 7) >> 3;
        static const uint64_t kSlotMask = 0xffffffffffffffff;
        static const size_t kPaddingBuckets_ =
                ((((kBytesPerBucket_ + 7) >> 3) << 3) - 1) / kBytesPerBucket_;


        struct Bucket {
            char bits_[kBytesPerBucket_];
        }__attribute__((__packed__));

        struct BucketForDynamic {
            BucketForDynamic* p;
            size_t index;  //imply the index of the last entry
            char bits_[kBytesPerBucket_*2];
            BucketForDynamic() : p(nullptr), index(0){
                memset(bits_,0, sizeof(bits_));
            }
            ~BucketForDynamic() = default;
        }__attribute__((__packed__));

        Bucket* buckets_;
        size_t num_buckets_;
        std::unordered_map<size_t,BucketForDynamic*> dynamic_part_;
        char* bitmap_;

        // we always transfer the head bucket's last entry to the empty slot
        uint32_t transfer(const size_t i){
            assert(BucketFull(i));
            BucketForDynamic* bucket = dynamic_part_[i];
            // here we minus the index by one, e.g. delete the last slot in bucket
            uint32_t res = ReadSlot(bucket->bits_,bucket->index--);
            if (bucket->index == 0){
                // empty bucket, delete it
                dynamic_part_[i] = bucket->p;
                delete bucket;
            }
            return res;
        }

    };
}
#endif //SB_KV_SINGLETABLE_H
