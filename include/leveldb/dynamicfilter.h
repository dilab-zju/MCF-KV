//
// Created by recross on 2022/9/30.
//

#ifndef SLM_DB_DYNAMICFILTER_H
#define SLM_DB_DYNAMICFILTER_H

#include "leveldb/export.h"
#include "status.h"
#include <vector>

namespace leveldb {


class LEVELDB_EXPORT DynamicFilter {
public:
    DynamicFilter() = default;
    virtual ~DynamicFilter() = default;
    //Returns the name of this filter
    virtual const char *Name() const = 0;

    // Insert a record into this filter
    virtual Status Add(uint64_t key, uint32_t file_no) = 0;

    // Checks if a record is inside this filter
    virtual Status Contain(const uint64_t key, std::vector<uint32_t>& vec) = 0;

    // Removes a key inside this filter
    virtual Status Delete(const uint64_t key, const uint32_t file_no) = 0;

};

LEVELDB_EXPORT DynamicFilter *NewCuckooFilter(const size_t num_max_key);

}

#endif //SLM_DB_DYNAMICFILTER_H
