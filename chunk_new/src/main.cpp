#include <string>
#include <stdio.h>
#include <stdlib.h>
#include "Chunker.hpp"
#include "sys/time.h"
#include "configure.hpp"
#include "keyClient.hpp"
#include "recvDecode.hpp"
#include "retriever.hpp"
//#include "sender.hpp"
//
//#include <bits/stdc++.h>
#include "boost/thread.hpp"
#include "dataSR.hpp"
#include "database.hpp"
#include "storageCore.hpp"
#include <signal.h>
#include <boost/thread/thread.hpp>

#define path "../test.jpg"

using namespace std;

Configure config("../config.json");
Chunker *chunkerObj;


Database fp2ChunkDB;
Database fileName2metaDB;

//DataSR *dataSRObj;
StorageCore *storageObj;

//keyClient *keyClientObj;
//Sender *senderObj;
RecvDecode *recvDecodeObj;
Retriever *retrieverObj;

struct timeval timestart;
struct timeval timeend;

void usage() {
    cout << "[client -r filename] for receive file" << endl;
    cout << "[client -s filename] for send file" << endl;
}

int main(int argv, char* argc[]) {
    vector<boost::thread *> thList;
    boost::thread *th;
    boost::thread::attributes attrs;
    attrs.set_stack_size(200 * 1024 * 1024);

    struct sigaction sa;
    sa.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &sa, 0);

    sigaction(SIGKILL, &sa, 0);
    sigaction(SIGINT, &sa, 0);

    fp2ChunkDB.openDB(config.getFp2ChunkDBName());
    fileName2metaDB.openDB(config.getFp2MetaDBame());

    storageObj = new StorageCore();
//    dataSRObj = new DataSR(storageObj_);
//

//
    if (strcmp("-r", argc[1]) == 0) {
        string fileName(argc[2]);
        printf("%s ++ %s\n",argc[1] , argc[2]);
        recvDecodeObj = new RecvDecode(fileName,storageObj);
        retrieverObj = new Retriever(fileName, recvDecodeObj,storageObj);

        for (int i = 0; i < config.getRecvDecodeThreadLimit(); i++) {
            th = new boost::thread(attrs, boost::bind(&RecvDecode::run, recvDecodeObj));
            thList.push_back(th);
        }
        th = new boost::thread(attrs, boost::bind(&Retriever::recvThread, retrieverObj));
        thList.push_back(th);
//
    } else if (strcmp("-s", argc[1]) == 0) {

//        senderObj = new Sender();
//        keyClientObj = new keyClient();

    string inputFile(argc[2]);
    chunkerObj = new Chunker(inputFile,storageObj);


    // start chunking thread
    th = new boost::thread(attrs, boost::bind(&Chunker::chunking, chunkerObj));
    thList.push_back(th);



//    th = new boost::thread(attrs, boost::bind(&DataSR::run, dataSRObj));
//    thList.push_back(th);

//        // start key client thread
//        for (int i = 0; i < config.getKeyClientThreadLimit(); i++) {
//            th = new boost::thread(attrs, boost::bind(&keyClient::run, keyClientObj));
//            thList.push_back(th);
//        }
//
//        // //start sender thread
//        for (int i = 0; i < config.getSenderThreadLimit(); i++) {
//            th = new boost::thread(attrs, boost::bind(&Sender::run, senderObj));
//            thList.push_back(th);
//        }
    } else {
        usage();
        return 0;
    }

    gettimeofday(&timestart, NULL);



    for (auto it : thList) {
        it->join();
    }

    if (storageObj != nullptr)
        delete storageObj;


    gettimeofday(&timeend, NULL);
    long diff = 1000000 * (timeend.tv_sec - timestart.tv_sec) + timeend.tv_usec - timestart.tv_usec;
    double second = diff / 1000000.0;
    cout << "the total work time is " << diff << " us = " << second << " s" << endl;
    return 0;
}
