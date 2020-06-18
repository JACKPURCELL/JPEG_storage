#ifndef GENERALDEDUPSYSTEM_DATASR_HPP
#define GENERALDEDUPSYSTEM_DATASR_HPP

#include "boost/thread.hpp"
#include "configure.hpp"
#include "dataStructure.hpp"
#include "messageQueue.hpp"
#include "protocol.hpp"
#include "storageCore.hpp"
#include <bits/stdc++.h>


using namespace std;

extern Configure config;

class DataSR {
  private:
    StorageCore *storageObj_;
    uint32_t restoreChunkBatchSize;

  public:
    DataSR(StorageCore *storageObj);
    ~DataSR(){};
};

#endif // GENERALDEDUPSYSTEM_DATASR_HPP
