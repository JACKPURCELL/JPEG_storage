#include <string>
#include <stdio.h>
#include <stdlib.h>
#include "Chunker.hpp"
#include "sys/time.h"
#include "configure.hpp"
#include "keyClient.hpp"
#include "recvDecode.hpp"
#include "retriever.hpp"
#include <dirent.h>

//#include "sender.hpp"
//
//#include <bits/stdc++.h>
#include "boost/thread.hpp"
#include "dataSR.hpp"
#include "database.hpp"
#include "storageCore.hpp"
#include <signal.h>
#include <boost/thread/thread.hpp>

#define FOLDER "/home/jpglittlerecode"


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

char basePath[1000];
char trainpath[1000];
char FullPathName[1024][1024];
long long JPEGFileNum=0;
long long workon=0;




//——————————————————————————————————————————————————————————————————————
//use for record all jpeg files in the dir
int readFileList(char *basePath,FILE* picname)
{

    DIR *dir;
    struct dirent *ptr;
    char base[1000];
    int namelen = 0;

    if ((dir=opendir(basePath)) == NULL)
    {
        printf("Open dir printf...");
        exit(1);
    }

    while ((ptr=readdir(dir)) != NULL)
    {
        if(strcmp(ptr->d_name,".")==0 || strcmp(ptr->d_name,"..")==0)    ///current dir OR parrent dir
            continue;
        else if(ptr->d_type == 8)
        {
            namelen = strlen(ptr->d_name);if(ptr->d_name[namelen-4] == '.' && ( ptr->d_name[namelen-3] == 'j' || ptr->d_name[namelen-3] == 'J' ) && ( ptr->d_name[namelen-2] == 'p' || ptr->d_name[namelen-2] == 'P' ) && ( ptr->d_name[namelen-1] == 'g' || ptr->d_name[namelen-1] == 'G' ))    ///jpgfile
            {

                char TempRecord[1000];
                memset(TempRecord, '\0', sizeof(TempRecord));
                strcat(TempRecord,basePath);
                strcat(TempRecord,"/");
                strcat(TempRecord,ptr->d_name);

                strcpy(FullPathName[JPEGFileNum++],TempRecord);

                memset(TempRecord, 0, sizeof(TempRecord));
                fprintf(picname,"%s\n",ptr->d_name);
            }
        }

        else if(ptr->d_type == 4)    ///dir
        {
            memset(base,'\0',sizeof(base));
            strcpy(base,basePath);
            strcat(base,"/");
            strcat(base,ptr->d_name);
            readFileList(base,picname);
        }
    }
    closedir(dir);
    return 1;
}

void getFilelist(){
    FILE *names;
    DIR *dir;

    memset(basePath, '\0', sizeof(basePath));
    strcpy(trainpath, basePath);
    strcat(trainpath, FOLDER);
    strcat(trainpath, "/train.txt");
    strcat(basePath, FOLDER);
    printf("%s\n", basePath);
    fflush(stdout);

    names = fopen(trainpath, "a+");
    readFileList(basePath, names);

    fclose(names);
}

void usage() {
    cout << "[client -r filename] for receive file" << endl;
    cout << "[client -s filename] for send file" << endl;
}


////用于测试，自动上传目录下全部文件
//int main(int argv, char* argc[]) {
//    fp2ChunkDB.openDB(config.getFp2ChunkDBName());
//    fileName2metaDB.openDB(config.getFp2MetaDBame());
//
//
//        getFilelist();
//
//        for(workon=0;workon<JPEGFileNum;workon++){
//            vector<boost::thread *> thList;
//            boost::thread *th;
//            boost::thread::attributes attrs;
//            attrs.set_stack_size(200 * 1024 * 1024);
//
//            struct sigaction sa;
//            sa.sa_handler = SIG_IGN;
//            sigaction(SIGPIPE, &sa, 0);
//
//            sigaction(SIGKILL, &sa, 0);
//            sigaction(SIGINT, &sa, 0);
//
//
//
//            storageObj = new StorageCore();
//
//
//            printf("\n%s\n",FullPathName[workon]);
//            string inputFile(FullPathName[workon]);
//            chunkerObj = new Chunker(inputFile,storageObj);
//
//            // start chunking thread
//            th = new boost::thread(attrs, boost::bind(&Chunker::chunking, chunkerObj));
//            thList.push_back(th);
//            gettimeofday(&timestart, NULL);
//
//
//
//            for (auto it : thList) {
//                it->join();
//            }
//
//            if (storageObj != nullptr)
//                delete storageObj;
//
//
//            gettimeofday(&timeend, NULL);
//            long diff = 1000000 * (timeend.tv_sec - timestart.tv_sec) + timeend.tv_usec - timestart.tv_usec;
//            double second = diff / 1000000.0;
//            cout << "the total work time is " << diff << " us = " << second << " s" << endl;
//        }
//
//
//    return 0;
//}
//








//用于正常使用
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

    } else if (strcmp("-s", argc[1]) == 0) {

    string inputFile(argc[2]);
    chunkerObj = new Chunker(inputFile,storageObj);

    // start chunking thread
    th = new boost::thread(attrs, boost::bind(&Chunker::chunking, chunkerObj));
    thList.push_back(th);

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
