#include "retriever.hpp"

Retriever::Retriever(string fileName, RecvDecode *&recvDecodeObjTemp,StorageCore* storageObj) {
    recvDecodeObj_ = recvDecodeObjTemp;
    storageObj_ = storageObj;
    string newFileName = fileName.append(".d");
    retrieveFile_.open(newFileName, ofstream::out | ofstream::binary);
    Recipe_t tempRecipe = recvDecodeObj_->getFileRecipeHead();
    totalChunkNumber_ = tempRecipe.fileRecipeHead.totalChunkNumber;
}

Retriever::~Retriever() {
    retrieveFile_.close();
}

void Retriever::recvThread() {
    Chunk_t newData;
    while (totalRecvNumber_ < totalChunkNumber_) {
        if (extractMQFromStorageCore(newData)) {
            retrieveFile_.write((char*)newData.logicData, newData.logicDataSize);
            totalRecvNumber_++;
        }
    }
    cout << "Retriever : job done, thread exit now" << endl;
    return;
}

bool Retriever::extractMQFromStorageCore(Chunk_t &newData) {
    return storageObj_->extractMQToRetriever(newData);
}
