//#ifndef GENERALDEDUPSYSTEM_KEYCLIENT_HPP
//#define GENERALDEDUPSYSTEM_KEYCLIENT_HPP
//
//#include "configure.h"
//#include "cryptoPrimitive.h"
//#include "dataStructure.h"
//#include "messageQueue.h"
//#include "sender.h"
//
//#define KEYMANGER_PUBLIC_KEY_FILE "key/serverpub.key"
//
//class keyClient {
//private:
//    messageQueue<Data_t> *inputMQ_;
//    Sender *senderObj_;
//    CryptoPrimitive *cryptoObj_;
//
//public:
//    keyClient(Sender *senderObjTemp);
//    ~keyClient();
//    void run();
//    bool encodeChunk(Data_t &newChunk);
//    bool insertMQFromChunker(Data_t &newChunk);
//    bool extractMQFromChunker(Data_t &newChunk);
//    bool insertMQToSender(Data_t &newChunk);
//    bool editJobDoneFlag();
//    bool setJobDoneFlag();
//};
//
//#endif // GENERALDEDUPSYSTEM_KEYCLIENT_HPP
