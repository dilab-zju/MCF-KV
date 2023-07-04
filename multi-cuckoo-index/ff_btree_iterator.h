//
// Created by recross on 2022/10/16.
//

#ifndef SB_KV_FF_BTREE_ITERATOR_H
#define SB_KV_FF_BTREE_ITERATOR_H

#include "ff_btreee.h"

namespace leveldb {

class FFBtreeIterator {
public:
    FFBtreeIterator(FFBtree* b);

    bool Valid() const;

    void SeekToFirst();

    void SeekToLast();

    void Seek(const entry_key_t& key);

    void Next();

    void Prev();

    entry_key_t key() const;

    void* value() const;

private:
    FFBtree* btree;
    Entry* cur;
    Page* cur_page;
    int index;
    bool valid; // validity of current entry
    bool last;
};

} // namespace leveldb


#endif //SB_KV_FF_BTREE_ITERATOR_H
