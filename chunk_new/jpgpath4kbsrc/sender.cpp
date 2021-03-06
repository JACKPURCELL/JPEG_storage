//#include "sender.h"
//#include <sys/time.h>
//
//extern Configure config;
//
//struct timeval timestartSender;
//struct timeval timeendSender;
//struct timeval timestartSenderRecipe;
//struct timeval timeendSenderRecipe;
//
//void PRINT_BYTE_ARRAY_SENDER(FILE *file, void *mem, uint32_t len) {
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
//Sender::Sender() {
//    inputMQ_ = new messageQueue<Data_t>(config.get_Data_t_MQSize());
//    socket_.init(CLIENT_TCP, config.getStorageServerIP(), config.getStorageServerPort());
//    cryptoObj_ = new CryptoPrimitive();
//    clientID_ = config.getClientID();
//}
//
//Sender::~Sender() {
//    socket_.finish();
//    if (cryptoObj_ != NULL) {
//        delete cryptoObj_;
//    }
//    inputMQ_->~messageQueue();
//    delete inputMQ_;
//}
//
//bool Sender::sendRecipe(Recipe_t request, RecipeList_t recipeList, int &status) {
//    int totalRecipeNumber = recipeList.size();
//    // cout << "Sender : Start sending file recipes, total recipe entry = " <<
//    // totalRecipeNumber << endl;
//    int sendRecipeNumber = 0;
//    int sendRecipeBatchNumber = config.getSendRecipeBatchSize();
//    int currentSendRecipeNumber = 0;
//    while ((totalRecipeNumber - sendRecipeNumber) != 0) {
//
//        if (totalRecipeNumber - sendRecipeNumber < sendRecipeBatchNumber) {
//            currentSendRecipeNumber = totalRecipeNumber - sendRecipeNumber;
//        } else {
//            currentSendRecipeNumber = sendRecipeBatchNumber;
//        }
//        NetworkHeadStruct_t requestBody, respondBody;
//
//        requestBody.clientID = clientID_;
//        requestBody.messageType = CLIENT_UPLOAD_RECIPE;
//        respondBody.clientID = 0;
//        respondBody.messageType = 0;
//        respondBody.dataSize = 0;
//        int sendSize = sizeof(NetworkHeadStruct_t) + sizeof(Recipe_t) + currentSendRecipeNumber * sizeof(RecipeEntry_t);
//        requestBody.dataSize = sizeof(Recipe_t) + currentSendRecipeNumber * sizeof(RecipeEntry_t);
//        u_char requestBuffer[sendSize];
//        memcpy(requestBuffer, &requestBody, sizeof(requestBody));
//        memcpy(requestBuffer + sizeof(NetworkHeadStruct_t), &request, sizeof(Recipe_t));
//        for (int i = 0; i < currentSendRecipeNumber; i++) {
//            memcpy(requestBuffer + sizeof(NetworkHeadStruct_t) + sizeof(Recipe_t) + i * sizeof(RecipeEntry_t),
//                   &recipeList[sendRecipeNumber + i], sizeof(RecipeEntry_t));
//        }
//        if (!socket_.Send(requestBuffer, sendSize)) {
//            cerr << "Sender : error sending file resipces, peer may close" << endl;
//            return false;
//        }
//        // cout << "Sender : send file reipce number = " << currentSendRecipeNumber
//        // << " done" << endl;
//        sendRecipeNumber += currentSendRecipeNumber;
//        currentSendRecipeNumber = 0;
//    }
//    return true;
//}
//
//bool Sender::sendChunkList(char *requestBufferIn, int sendBufferSize, int sendChunkNumber, int &status) {
//    NetworkHeadStruct_t requestBody, respondBody;
//    requestBody.clientID = clientID_;
//    requestBody.messageType = CLIENT_UPLOAD_CHUNK;
//    respondBody.clientID = 0;
//    respondBody.messageType = 0;
//    respondBody.dataSize = 0;
//    u_char requestBuffer[NETWORK_MESSAGE_DATA_SIZE];
//    int sendSize = sizeof(NetworkHeadStruct_t) + sizeof(int) + sendBufferSize;
//    memcpy(requestBufferIn + sizeof(NetworkHeadStruct_t), &sendChunkNumber, sizeof(int));
//    requestBody.dataSize = sendBufferSize + sizeof(int);
//    memcpy(requestBufferIn, &requestBody, sizeof(NetworkHeadStruct_t));
//    u_char respondBuffer[sizeof(NetworkHeadStruct_t)];
//    int recvSize = 0;
//    if (!socket_.Send((u_char *)requestBufferIn, sendSize)) {
//        return false;
//    } else {
//        return true;
//    }
//}
//
//bool Sender::sendData(u_char *request, int requestSize, u_char *respond, int &respondSize, bool recv) {
//    std::lock_guard<std::mutex> locker(mutexSocket_);
//    if (!socket_.Send(request, requestSize)) {
//        cerr << "Sender : send data error peer closed" << endl;
//        return false;
//    }
//    if (recv) {
//        if (!socket_.Recv(respond, respondSize)) {
//            cerr << "Sender : recv data error peer closed" << endl;
//            return false;
//        }
//    }
//    return true;
//}
//
//bool Sender::sendEndFlag() {
//    NetworkHeadStruct_t requestBody;
//    requestBody.messageType = CLIENT_EXIT;
//    requestBody.clientID = clientID_;
//    int sendSize = sizeof(NetworkHeadStruct_t);
//    requestBody.dataSize = 0;
//    u_char requestBuffer[sendSize];
//    memcpy(requestBuffer, &requestBody, sizeof(NetworkHeadStruct_t));
//    if (!socket_.Send(requestBuffer, sendSize)) {
//        cerr << "Sender : send data error peer closed" << endl;
//        return false;
//    }
//    return true;
//}
//
//void Sender::run() {
//    // double totalSendTime = 0;
//    gettimeofday(&timestartSender, NULL);
//    Data_t tempChunk;
//    RecipeList_t recipeList;
//    Recipe_t fileRecipe;
//    int sendBatchSize = config.getSendChunkBatchSize();
//    int status;
//    char *sendChunkBatchBuffer =
//            (char *)malloc(sizeof(NetworkHeadStruct_t) + sizeof(int) +
//                           sizeof(char) * sendBatchSize * (CHUNK_HASH_SIZE + MAX_CHUNK_SIZE + sizeof(int)));
//    bool jobDoneFlag = false;
//    int currentChunkNumber = 0;
//    int currentSendRecipeNumber = 0;
//    int currentSendChunkBatchBufferSize = sizeof(NetworkHeadStruct_t) + sizeof(int);
//    while (!jobDoneFlag) {
//        if (inputMQ_->done_ && inputMQ_->isEmpty()) {
//            jobDoneFlag = true;
//        }
//        if (extractMQFromKeyClient(tempChunk)) {
//            if (tempChunk.dataType == DATA_TYPE_RECIPE) {
//                // cout << "Sender : get file recipe head, file size = " <<
//                // tempChunk.recipe.fileRecipeHead.fileSize << " file chunk number = "
//                // << tempChunk.recipe.fileRecipeHead.totalChunkNumber << endl;
//                // PRINT_BYTE_ARRAY_SENDER(stderr,
//                // tempChunk.recipe.fileRecipeHead.fileNameHash, FILE_NAME_HASH_SIZE);
//                memcpy(&fileRecipe, &tempChunk.recipe, sizeof(Recipe_t));
//                continue;
//            } else {
//
//                memcpy(sendChunkBatchBuffer + currentSendChunkBatchBufferSize, tempChunk.chunk.chunkHash,
//                       CHUNK_HASH_SIZE);
//                currentSendChunkBatchBufferSize += CHUNK_HASH_SIZE;
//                memcpy(sendChunkBatchBuffer + currentSendChunkBatchBufferSize, &tempChunk.chunk.logicDataSize,
//                       sizeof(int));
//                currentSendChunkBatchBufferSize += sizeof(int);
//                memcpy(sendChunkBatchBuffer + currentSendChunkBatchBufferSize, tempChunk.chunk.logicData,
//                       tempChunk.chunk.logicDataSize);
//                currentSendChunkBatchBufferSize += tempChunk.chunk.logicDataSize;
//                currentChunkNumber++;
//
//                RecipeEntry_t newRecipeEntry;
//                newRecipeEntry.chunkID = tempChunk.chunk.ID;
//                newRecipeEntry.chunkSize = tempChunk.chunk.logicDataSize;
//                memcpy(newRecipeEntry.chunkHash, tempChunk.chunk.chunkHash, CHUNK_HASH_SIZE);
//                memcpy(newRecipeEntry.chunkKey, tempChunk.chunk.encryptKey, CHUNK_ENCRYPT_KEY_SIZE);
//                recipeList.push_back(newRecipeEntry);
//                currentSendRecipeNumber++;
//            }
//        }
//        if (currentChunkNumber == sendBatchSize || jobDoneFlag) {
//            // cout << "Sender : run -> start send " << setbase(10) <<
//            // currentChunkNumber << " chunks to server, size = " << setbase(10) <<
//            // currentSendChunkBatchBufferSize << endl;
//            // gettimeofday(&timestartSenderRecipe, NULL);
//            if (this->sendChunkList(sendChunkBatchBuffer, currentSendChunkBatchBufferSize, currentChunkNumber,
//                                    status)) {
//                // cout << "Sender : sent " << setbase(10) << currentChunkNumber << "
//                // chunk" << endl;
//                currentSendChunkBatchBufferSize = sizeof(NetworkHeadStruct_t) + sizeof(int);
//                memset(sendChunkBatchBuffer, 0,
//                       sizeof(NetworkHeadStruct_t) + sizeof(int) +
//                       sizeof(char) * sendBatchSize * (CHUNK_HASH_SIZE + MAX_CHUNK_SIZE + sizeof(int)));
//                currentChunkNumber = 0;
//            } else {
//                cerr << "Sender : send " << setbase(10) << currentChunkNumber << " chunk error" << endl;
//                break;
//            }
//            // gettimeofday(&timeendSenderRecipe, NULL);
//            // long diff = 1000000 * (timeendSenderRecipe.tv_sec -
//            // timestartSenderRecipe.tv_sec) + timeendSenderRecipe.tv_usec -
//            // timestartSenderRecipe.tv_usec; double second = diff / 1000000.0;
//            // totalSendTime += second;
//        }
//    }
//    // printf("Sender send chunk list time is %lf s\n", totalSendTime);
//
//    // gettimeofday(&timestartSenderRecipe, NULL);
//    if (!this->sendRecipe(fileRecipe, recipeList, status)) {
//        cout << "Sender : send recipe list error, upload fail " << endl;
//        free(sendChunkBatchBuffer);
//        sendEndFlag();
//        gettimeofday(&timeendSender, NULL);
//        long diff =
//                1000000 * (timeendSender.tv_sec - timestartSender.tv_sec) + timeendSender.tv_usec - timestartSender.tv_usec;
//        double second = diff / 1000000.0;
//        cout << "Sender thread work time is " << diff << " us = " << second << " s" << endl;
//        return;
//    }
//    // gettimeofday(&timeendSenderRecipe, NULL);
//    // long diff = 1000000 * (timeendSenderRecipe.tv_sec -
//    // timestartSenderRecipe.tv_sec) + timeendSenderRecipe.tv_usec -
//    // timestartSenderRecipe.tv_usec; double second = diff / 1000000.0;
//    // printf("Sender send recipe list time is %ld us = %lf s\n", diff, second);
//
//    free(sendChunkBatchBuffer);
//    sendEndFlag();
//    gettimeofday(&timeendSender, NULL);
//    long diff =
//            1000000 * (timeendSender.tv_sec - timestartSender.tv_sec) + timeendSender.tv_usec - timestartSender.tv_usec;
//    double second = diff / 1000000.0;
//    cout << "Sender thread work time is " << diff << " us = " << second << " s" << endl;
//    return;
//}
//
//bool Sender::insertMQFromKeyClient(Data_t &newChunk) {
//    return inputMQ_->push(newChunk);
//}
//
//bool Sender::extractMQFromKeyClient(Data_t &newChunk) {
//    return inputMQ_->pop(newChunk);
//}
//
//bool Sender::editJobDoneFlag() {
//    inputMQ_->done_ = true;
//    if (inputMQ_->done_) {
//        return true;
//    } else {
//        return false;
//    }
//}
