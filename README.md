# MCF-KV

Based on original source of LevelDB <https://github.com/google/leveldb>

### Dependencies

- cmake
- DMDK - https://github.com/pmem/pmdk
- ndctl - https://github.com/pmem/ndctl

### Installation
```
mkdir build
cd build
cmake ..
make -j
```
### bloom_bench
Test Bloom filters's performance ,please modify paramters in 'bloom.cc'
```
bloom_nvm_dir          # path to PMEM directory
pmem_test              # if we run bloom bench, set it to true
```

### btree_bench
Test FFBtree's performance
```
nvm_dir               # path to PMEM directory
nvm_size              # NVM pool size
```

### cuckoo_bench
Test Cuckoo filters performance, please configure paramters in 'multi-cuckoo-index/index.cc'
```
CuckooFilterNum      # number of cuckoo filters
max_num_keys         # maxkeys a filter can maintain, which determins the size of hash table
merely_cuckoo_filter # if set true, Cuckoo filter will execute 50us sleep to simulate IO operation when encounter hash collsion
```

### db_bench
db_bench is used to test the overall performance of MCF-KV
