#ifndef STORAGE_LEVELDB_INCLUDE_INDEX_H_
#define STORAGE_LEVELDB_INCLUDE_INDEX_H_

#include <cstdint>
#include <memory>
#include <deque>
#include "leveldb/slice.h"
#include "leveldb/iterator.h"
#include "leveldb/options.h"
#include "leveldb/string.h"

using entry_key_t = uint64_t;

namespace leveldb {

class TableCache;
class VersionEdit;
class VersionControl;

struct IndexMeta {
public:
  uint32_t file_number;

  IndexMeta() : file_number(0) { }

  IndexMeta(uint32_t file_number) :
     file_number(file_number) { }

};

extern bool IsEqual(const IndexMeta* lhs, const IndexMeta* rhs);

struct KeyAndMeta{
  entry_key_t key;
  std::shared_ptr<IndexMeta> meta;
};

class Index {
public:
  Index() = default;
  virtual ~Index() = default;
  virtual Status Insert(const entry_key_t& key, const IndexMeta& meta) = 0;
  //virtual void Insert(const uint32_t& key, IndexMeta meta) = 0;
  virtual uint32_t Get(const Slice& key) = 0;
  virtual void AddQueue(std::deque<KeyAndMeta>& queue, VersionEdit* edit, bool for_update) = 0;
  virtual Iterator* NewIterator(const ReadOptions& options, TableCache* table_cache, VersionControl* vcontrol) = 0;
  virtual void Break() = 0;
};

Index* CreateBtreeIndex();
Index* CreateMCFIndex();

} // namespace leveldb

#endif //STORAGE_LEVELDB_INCLUDE_INDEX_H_
