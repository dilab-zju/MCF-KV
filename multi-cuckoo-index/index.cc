//
// Created by recross on 2022/10/19.
//

#include "leveldb/index.h"
#include "multi_cuckoo_index.h"
namespace leveldb{
const int CuckooFilterNum = 10;
Index* CreateMCFIndex(){
    return new MultiCuckooIndex("test",CuckooFilterNum);
}
}

