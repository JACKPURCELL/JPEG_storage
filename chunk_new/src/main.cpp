#include <string>
#include <stdio.h>
#include <stdlib.h>
#include "Chunker.h"
#include "sys/time.h"
#include "configure.h"
#include "keyClient.h"
//#include "recvDecode.h"
//#include "retriever.hpp"
//#include "sender.hpp"
//
//#include <bits/stdc++.h>
#include <boost/thread/thread.hpp>

#define path "../test.jpg"

using namespace std;

Configure config("config.json");
Chunker *chunkerObj;
//keyClient *keyClientObj;
//Sender *senderObj;
//RecvDecode *recvDecodeObj;
//Retriever *retrieverObj;

struct timeval timestart;
struct timeval timeend;

//void usage() {
//    cout << "[client -r filename] for receive file" << endl;
//    cout << "[client -s filename] for send file" << endl;
//}

int main() {
    vector<boost::thread *> thList;
    boost::thread *th;

    boost::thread::attributes attrs;
    attrs.set_stack_size(200 * 1024 * 1024);
//
//    if (strcmp("-r", argc[1]) == 0) {
//        string fileName(argc[2]);
//
//        recvDecodeObj = new RecvDecode(fileName);
//        retrieverObj = new Retriever(fileName, recvDecodeObj);
//
//        for (int i = 0; i < config.getRecvDecodeThreadLimit(); i++) {
//            th = new boost::thread(attrs, boost::bind(&RecvDecode::run, recvDecodeObj));
//            thList.push_back(th);
//        }
//        th = new boost::thread(attrs, boost::bind(&Retriever::recvThread, retrieverObj));
//        thList.push_back(th);
//
//    } else if (strcmp("-s", argc[1]) == 0) {

//        senderObj = new Sender();
//        keyClientObj = new keyClient();
    string inputFile(path);
    chunkerObj = new Chunker(inputFile);


        // start chunking thread
        th = new boost::thread(attrs, boost::bind(&Chunker::chunking, chunkerObj));
        thList.push_back(th);

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
//    } else {
//        usage();
//        return 0;
//    }
//
    gettimeofday(&timestart, NULL);


        thList[0]->join();

    gettimeofday(&timeend, NULL);
    long diff = 1000000 * (timeend.tv_sec - timestart.tv_sec) + timeend.tv_usec - timestart.tv_usec;
    double second = diff / 1000000.0;
    cout << "the total work time is " << diff << " us = " << second << " s" << endl;
    return 0;
}