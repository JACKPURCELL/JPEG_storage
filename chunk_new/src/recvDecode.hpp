#ifndef GENERALDEDUPSYSTEM_RECIVER_HPP
#define GENERALDEDUPSYSTEM_RECIVER_HPP

#include "configure.hpp"
#include "cryptoPrimitive.hpp"
#include "dataStructure.hpp"
#include "messageQueue.hpp"
#include "protocol.hpp"
#include "storageCore.hpp"
#include <bits/stdc++.h>

using namespace std;

class RecvDecode {
  private:
    StorageCore *storageObj_;

    void receiveChunk();
    u_char fileNameHash_[FILE_NAME_HASH_SIZE];
    CryptoPrimitive *cryptoObj_;
    messageQueue<Chunk_t> *outPutMQ_;
    std::mutex multiThreadDownloadMutex;
    uint32_t totalRecvChunks = 0;
    int clientID_;
    uint32_t restoreChunkBatchSize;


public:
    Recipe_t fileRecipe_;
    RecvDecode(string fileName,StorageCore* storageObj);
    ~RecvDecode();
    void run();
    bool recvFileHead(Recipe_t &FileRecipe, char * fileNameHash);
    bool recvChunks(ChunkList_t &recvChunk, int &chunkNumber, uint32_t &startID, uint32_t &endID);
    Recipe_t getFileRecipeHead();
    bool insertMQToRetriever(Chunk_t &newChunk);
    bool extractMQFromRecvDecode(Chunk_t &newChunk);
};

#endif // GENERALDEDUPSYSTEM_RECIVER_HPP
