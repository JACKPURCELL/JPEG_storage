////
//// Created by 梁嘉城 on 2020/3/27.
////
//
//#ifndef CHUNK_NEW_SENDER_H
//#define CHUNK_NEW_SENDER_H
//
//
//
//#include "configure.h"
//#include "cryptoPrimitive.h"
//#include "dataStructure.h"
//#include "messageQueue.h"
//#include "protocol.hpp"
//#include "socket.hpp"
//
//class Sender {
//private:
//    std::mutex mutexSocket_;
//    Socket socket_;
//    int clientID_;
//    messageQueue<Data_t> *inputMQ_;
//    CryptoPrimitive *cryptoObj_;
//
//public:
//    Sender();
//
//    ~Sender();
//
//    // status define in protocol.hpp
//    bool sendRecipe(Recipe_t request, RecipeList_t requestList, int &status);
//    bool sendChunkList(ChunkList_t request, int &status);
//    bool sendChunkList(char *requestBufferIn, int sendBufferSize, int sendChunkNumber, int &status);
//
//    // send chunk when socket free
//    void run();
//
//    // general send data
//    bool sendData(u_char *request, int requestSize, u_char *respond, int &respondSize, bool recv);
//    bool sendEndFlag();
//    bool insertMQFromKeyClient(Data_t &newChunk);
//    bool extractMQFromKeyClient(Data_t &newChunk);
//    bool editJobDoneFlag();
//};
//
//#endif // GENERALDEDUPSYSTEM_SENDER_HPP
