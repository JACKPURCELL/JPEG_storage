#include "recvDecode.hpp"

extern Configure config;

struct timeval timestartDataSR;
struct timeval timeendDataSR;

void PRINT_BYTE_ARRAY_RECV(FILE *file, void *mem, uint32_t len) {
    if (!mem || !len) {
        fprintf(file, "\n( null )\n");
        return;
    }
    uint8_t *array = (uint8_t *)mem;
    fprintf(file, "%u bytes:\n{\n", len);
    uint32_t i = 0;
    for (i = 0; i < len - 1; i++) {
        fprintf(file, "0x%x, ", array[i]);
        if (i % 8 == 7) fprintf(file, "\n");
    }
    fprintf(file, "0x%x ", array[i]);
    fprintf(file, "\n}\n");
}

RecvDecode::RecvDecode(string fileName,StorageCore* storageObj) {
//    clientID_ = config.getClientID();
//    outPutMQ_ = new messageQueue<RetrieverData_t>(config.get_RetrieverData_t_MQSize());
    cryptoObj_ = new CryptoPrimitive();
//    socket_.init(CLIENT_TCP, config.getStorageServerIP(), config.getStorageServerPort());
    storageObj_ = storageObj;
    cryptoObj_->generateHash((u_char *)&fileName[0], fileName.length(), fileNameHash_);
    restoreChunkBatchSize = config.getSendChunkBatchSize();
    recvFileHead(fileRecipe_, (char*)fileNameHash_);
    cerr << "RecvDecode : recv file recipe head, file size = " << fileRecipe_.fileRecipeHead.fileSize
         << ", total chunk number = " << fileRecipe_.fileRecipeHead.totalChunkNumber << endl;
}

RecvDecode::~RecvDecode() {
    if (cryptoObj_ != nullptr) {
        delete cryptoObj_;
    }
    outPutMQ_->~messageQueue();
    delete outPutMQ_;
}

bool RecvDecode::recvFileHead(Recipe_t &fileRecipe, char * fileNameHash) {

    if (storageObj_->restoreRecipeHead(fileNameHash, fileRecipe)) {
        cout << "StorageCore : restore file size = " << fileRecipe.fileRecipeHead.fileSize << " chunk number = " << fileRecipe.fileRecipeHead.totalChunkNumber << endl;
        cout << "StorageCore : success find the recipe" << endl;
    } else {
        cerr << "StorageCore : file not exist" << endl;
    }

    return true;
}

//bool RecvDecode::recvChunks(ChunkList_t &recvChunk, int &chunkNumber, uint32_t &startID, uint32_t &endID) {
//    NetworkHeadStruct_t request, respond;
//    request.messageType = CLIENT_DOWNLOAD_CHUNK_WITH_RECIPE;
//    request.dataSize = FILE_NAME_HASH_SIZE + 2 * sizeof(uint32_t);
//    request.clientID = clientID_;
//    int sendSize = sizeof(NetworkHeadStruct_t) + FILE_NAME_HASH_SIZE + 2 * sizeof(uint32_t);
//    u_char requestBuffer[sendSize];
//    u_char respondBuffer[NETWORK_RESPOND_BUFFER_MAX_SIZE];
//    int recvSize;
//    memcpy(requestBuffer, &request, sizeof(NetworkHeadStruct_t));
//    memcpy(requestBuffer + sizeof(NetworkHeadStruct_t), fileNameHash_, FILE_NAME_HASH_SIZE);
//    memcpy(requestBuffer + sizeof(NetworkHeadStruct_t) + FILE_NAME_HASH_SIZE, &startID, sizeof(uint32_t));
//    memcpy(requestBuffer + sizeof(NetworkHeadStruct_t) + FILE_NAME_HASH_SIZE + sizeof(uint32_t), &endID,
//           sizeof(uint32_t));
//    while (true) {
//        if (!socket_.Send(requestBuffer, sendSize)) {
//            cerr << "RecvDecode : storage server closed" << endl;
//            return false;
//        }
//        if (!socket_.Recv(respondBuffer, recvSize)) {
//            cerr << "RecvDecode : storage server closed" << endl;
//            return false;
//        }
//        memcpy(&respond, respondBuffer, sizeof(NetworkHeadStruct_t));
//        if (respond.messageType == ERROR_RESEND) continue;
//        if (respond.messageType == ERROR_CLOSE) {
//            cerr << "RecvDecode : Server reject download request!" << endl;
//            exit(1);
//        }
//        if (respond.messageType == SUCCESS) {
//            chunkNumber = endID - startID;
//            int totalRecvSize = 0;
//            for (int i = 0; i < chunkNumber; i++) {
//                Chunk_t tempChunk;
//                memcpy(&tempChunk.ID, respondBuffer + sizeof(NetworkHeadStruct_t) + totalRecvSize, sizeof(uint32_t));
//                totalRecvSize += sizeof(uint32_t);
//                memcpy(&tempChunk.logicDataSize, respondBuffer + sizeof(NetworkHeadStruct_t) + totalRecvSize,
//                       sizeof(int));
//                totalRecvSize += sizeof(int);
//                memcpy(&tempChunk.logicData, respondBuffer + sizeof(NetworkHeadStruct_t) + totalRecvSize,
//                       tempChunk.logicDataSize);
//                totalRecvSize += tempChunk.logicDataSize;
//                memcpy(&tempChunk.encryptKey, respondBuffer + sizeof(NetworkHeadStruct_t) + totalRecvSize,
//                       CHUNK_ENCRYPT_KEY_SIZE);
//                totalRecvSize += CHUNK_ENCRYPT_KEY_SIZE;
//                recvChunk.push_back(tempChunk);
//            }
//            break;
//        }
//    }
//    return true;
//}

Recipe_t RecvDecode::getFileRecipeHead() {
    return fileRecipe_;
}

bool RecvDecode::insertMQToRetriever(Chunk_t &newChunk) {
    return outPutMQ_->push(newChunk);
}
bool RecvDecode::extractMQFromRecvDecode(Chunk_t &newChunk){
    return outPutMQ_->pop(newChunk);
}

void RecvDecode::run() {
    uint32_t totalRestoredChunkNumber = 0;
    uint32_t startID = 0;
    uint32_t endID = 0;


    if (fileRecipe_.fileRecipeHead.totalChunkNumber < config.getSendChunkBatchSize()) {
        endID = fileRecipe_.fileRecipeHead.totalChunkNumber - 1;
    }
    while (totalRestoredChunkNumber != fileRecipe_.fileRecipeHead.totalChunkNumber) {
        ChunkList_t restoredChunkList;
        gettimeofday(&timestartDataSR, NULL);
        if (storageObj_->restoreRecipeAndChunk((char*)fileNameHash_, startID, endID, restoredChunkList)) {
            totalRestoredChunkNumber += restoredChunkList.size();
            startID = endID;
            if (fileRecipe_.fileRecipeHead.totalChunkNumber - totalRestoredChunkNumber < restoreChunkBatchSize) {
                endID += fileRecipe_.fileRecipeHead.totalChunkNumber - totalRestoredChunkNumber;
            } else {
                endID += config.getSendChunkBatchSize();
            }
        } else {
            cerr << "RecvDecode : chunk not exist" << endl;
            return;
        }
        gettimeofday(&timeendDataSR, NULL);
        int diff = 1000000 * (timeendDataSR.tv_sec - timestartDataSR.tv_sec) + timeendDataSR.tv_usec - timestartDataSR.tv_usec;
        double second = diff / 1000000.0;
        cout << "DataSR : restore chunk time  = " << second << endl;
    }

    cerr << "RecvDecode : download job done, exit now" << endl;
    return;
}
