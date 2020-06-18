#ifndef GENERALDEDUPSYSTEM_RETRIEVER_HPP
#define GENERALDEDUPSYSTEM_RETRIEVER_HPP

#include "configure.hpp"
#include "cryptoPrimitive.hpp"
#include "dataStructure.hpp"
#include "messageQueue.hpp"
#include "protocol.hpp"
#include "recvDecode.hpp"
#include <bits/stdc++.h>
#include <boost/thread.hpp>

using namespace std;

class Retriever {
  private:
    int chunkCnt_;
    std::ofstream retrieveFile_;
    RecvDecode *recvDecodeObj_;
    StorageCore* storageObj_;
    // unordered_map<uint32_t, string> chunkTempList_;
    std::mutex multiThreadWriteMutex;
    uint32_t currentID_ = 0;
    uint32_t totalChunkNumber_;
    uint32_t totalRecvNumber_ = 0;

  public:
    Retriever(string fileName, RecvDecode *&recvDecodeObjTemp,StorageCore* storageObj);
    ~Retriever();
    void recvThread();
    void retrieveFileThread();
    bool extractMQFromStorageCore(Chunk_t &newData);
};

#endif // GENERALDEDUPSYSTEM_RETRIEVER_HPP
