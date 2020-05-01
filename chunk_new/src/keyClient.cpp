//#include "keyClient.hpp"
//#include "openssl/rsa.h"
//#include "configure.h"
//#include <sys/time.h>
//
//extern Configure config;
//
//struct timeval timestartKey;
//struct timeval timeendKey;
//
//void PRINT_BYTE_ARRAY_KEY_CLIENT(FILE *file, void *mem, uint32_t len) {
//    if (!mem || !len) {
//        fprintf(file, "\n( null )\n");
//        return;
//    }
//    uint8_t *array = (uint8_t *)mem;
//    fprintf(file, "%u bytes:\n{\n", len);
//    uint32_t i = 0;
//    for (i = 0; i < len - 1; i++) {
//        fprintf(file, "0x%x, ", array[i]);
//        if (i % 8 == 7) fprintf(file, "\n");
//    }
//    fprintf(file, "0x%x ", array[i]);
//    fprintf(file, "\n}\n");
//}
//
//keyClient::keyClient(Sender *senderObjTemp) {
//    inputMQ_ = new messageQueue<Data_t>(config.get_Data_t_MQSize());
//    senderObj_ = senderObjTemp;
//    cryptoObj_ = new CryptoPrimitive();
//}
//
//keyClient::~keyClient() {
//    if (cryptoObj_ != NULL) {
//        delete cryptoObj_;
//    }
//    inputMQ_->~messageQueue();
//    delete inputMQ_;
//}
//
//void keyClient::run() {
//    gettimeofday(&timestartKey, NULL);
//    bool JobDoneFlag = false;
//    while (true) {
//        Data_t tempChunk;
//        if (inputMQ_->done_ && inputMQ_->isEmpty()) {
//            // cerr << "KeyClient : Chunker jobs done, queue is empty" << endl;
//            JobDoneFlag = true;
//        }
//        if (extractMQFromChunker(tempChunk)) {
//            if (tempChunk.dataType == DATA_TYPE_RECIPE) {
//                insertMQToSender(tempChunk);
//                continue;
//            } else {
//                memcpy(tempChunk.chunk.encryptKey, tempChunk.chunk.chunkHash, CHUNK_HASH_SIZE);
//                if (encodeChunk(tempChunk)) {
//                    insertMQToSender(tempChunk);
//                } else {
//                    cerr << "KeyClient : encode chunk error, exiting" << endl;
//                    return;
//                }
//            }
//        }
//
//        if (JobDoneFlag) {
//            if (!senderObj_->editJobDoneFlag()) {
//                cerr << "KeyClient : error to set job done flag for sender" << endl;
//            }
//            break;
//        }
//    }
//
//    gettimeofday(&timeendKey, NULL);
//    long diff = 1000000 * (timeendKey.tv_sec - timestartKey.tv_sec) + timeendKey.tv_usec - timestartKey.tv_usec;
//    double second = diff / 1000000.0;
//    printf("Key client thread work time is %ld us = %lf s\n", diff, second);
//    return;
//}
//
//bool keyClient::encodeChunk(Data_t &newChunk) {
//    bool statusChunk = cryptoObj_->encryptChunk(newChunk.chunk);
//    bool statusHash = cryptoObj_->generateHash(newChunk.chunk.logicData, newChunk.chunk.logicDataSize, newChunk.chunk.chunkHash);
//    if (!statusChunk) {
//        cerr << "KeyClient : error encrypt chunk" << endl;
//        return false;
//    } else if (!statusHash) {
//        cerr << "KeyClient : error compute hash" << endl;
//        return false;
//    } else {
//        return true;
//    }
//}
//
//bool keyClient::insertMQFromChunker(Data_t &newChunk) {
//    return inputMQ_->push(newChunk);
//}
//
//bool keyClient::extractMQFromChunker(Data_t &newChunk) {
//    return inputMQ_->pop(newChunk);
//}
//
//bool keyClient::insertMQToSender(Data_t &newChunk) {
//    return senderObj_->insertMQFromKeyClient(newChunk);
//}
//
//bool keyClient::editJobDoneFlag() {
//    inputMQ_->done_ = true;
//    if (inputMQ_->done_) {
//        return true;
//    } else {
//        return false;
//    }
//}
