//
// Created by 梁嘉城 on 2020/3/19.
//

#include "Chunker.hpp"

#include <sys/time.h>


extern Configure config;
extern Database fp2ChunkDB;
extern Database fileName2metaDB;

struct timeval timestartChunker;
struct timeval timeendChunker;
int chunkcount2=0;
//messageQueue<Data_t> *inputMQ_;

void PRINT_BYTE_ARRAY_CHUNKER(FILE *file, void *mem, uint32_t len) {
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

Chunker::Chunker(std::string path,StorageCore* storageObj) {
    loadChunkFile(path);
    ChunkerInit(path);
    cryptoObj = new CryptoPrimitive();
    storageObj_ = storageObj;
//    dataSRObj = new DataSR(storageObj_);

//    keyClientObj = keyClientObjTemp;
}

std::ifstream &Chunker::getChunkingFile() {
    if (!chunkingFile.is_open()) {
        cerr << "Chunker : chunking file open failed" << endl;
        exit(1);
    }
    return chunkingFile;
}

void Chunker::loadChunkFile(std::string path) {
    if (chunkingFile.is_open()) {
        chunkingFile.close();
    }
    chunkingFile.open(path, std::ios::binary);
    if (!chunkingFile.is_open()) {
        cerr << "Chunker : open file: " << path << "error" << endl;
        exit(1);
    }
}

Chunker::~Chunker() {
    if (powerLUT != NULL) {
        delete powerLUT;
    }
    if (removeLUT != NULL) {
        delete removeLUT;
    }
    if (waitingForChunkingBuffer != NULL) {
        delete waitingForChunkingBuffer;
    }
    if (chunkBuffer != NULL) {
        delete chunkBuffer;
    }
//    if (cryptoObj != NULL) {
//        delete cryptoObj;
//    }
    if (chunkingFile.is_open()) {
        chunkingFile.close();
    }
    if (storageObj_ != nullptr)
        delete storageObj_;

}

void Chunker::ChunkerInit(string path) {
    u_char filePathHash[FILE_NAME_HASH_SIZE];
    cryptoObj->generateHash((u_char *)&path[0], path.length(), filePathHash);//计算hash
    memcpy(recipe.recipe.fileRecipeHead.fileNameHash, filePathHash, FILE_NAME_HASH_SIZE);
    memcpy(recipe.recipe.keyRecipeHead.fileNameHash, filePathHash, FILE_NAME_HASH_SIZE);

    ChunkerType = (int)config.getChunkingType();

    if (ChunkerType == CHUNKER_VAR_SIZE_TYPE) {

        int numOfMaskBits;
        avgChunkSize = (int)config.getAverageChunkSize();
        minChunkSize = (int)config.getMinChunkSize();
        maxChunkSize = (int)config.getMaxChunkSize();
        slidingWinSize = (int)config.getSlidingWinSize();
        ReadSize = config.getReadSize();
        ReadSize = ReadSize * 1024 * 1024;
        waitingForChunkingBuffer = new u_char[ReadSize];
        chunkBuffer = new u_char[maxChunkSize];

        if (waitingForChunkingBuffer == NULL || chunkBuffer == NULL) {
            cerr << "Chunker : Memory malloc error" << endl;
            exit(1);
        }
        if (minChunkSize >= avgChunkSize) {
            cerr << "Chunker : minChunkSize should be smaller than avgChunkSize!" << endl;
            exit(1);
        }
        if (maxChunkSize <= avgChunkSize) {
            cerr << "Chunker : maxChunkSize should be larger than avgChunkSize!" << endl;
            exit(1);
        }

        /*initialize the base and modulus for calculating the fingerprint of a
         * window*/
        /*these two values were employed in open-vcdiff:
         * "http://code.google.com/p/open-vcdiff/"*/
        polyBase = 257; /*a prime larger than 255, the max value of "unsigned char"*/
        polyMOD = (1 << 23) - 1; /*polyMOD - 1 = 0x7fffff: use the last 23 bits of a
                                    polynomial as its hash*/
        /*initialize the lookup table for accelerating the power calculation in
         * rolling hash*/
        powerLUT = (uint32_t *)malloc(sizeof(uint32_t) * slidingWinSize);
        /*powerLUT[i] = power(polyBase, i) mod polyMOD*/
        powerLUT[0] = 1;
        for (int i = 1; i < slidingWinSize; i++) {
            /*powerLUT[i] = (powerLUT[i-1] * polyBase) mod polyMOD*/
            powerLUT[i] = (powerLUT[i - 1] * polyBase) & polyMOD;
        }
        /*initialize the lookup table for accelerating the byte remove in rolling
         * hash*/
        removeLUT = (uint32_t *)malloc(sizeof(uint32_t) * 256); /*256 for unsigned char*/
        for (int i = 0; i < 256; i++) {
            /*removeLUT[i] = (- i * powerLUT[_slidingWinSize-1]) mod polyMOD*/
            removeLUT[i] = (i * powerLUT[slidingWinSize - 1]) & polyMOD;
            if (removeLUT[i] != 0) {

                removeLUT[i] = (polyMOD - removeLUT[i] + 1) & polyMOD;
            }
            /*note: % is a remainder (rather than modulus) operator*/
            /*      if a < 0,  -polyMOD < a % polyMOD <= 0       */
        }

        /*initialize the anchorMask for depolytermining an anchor*/
        /*note: power(2, numOfanchorMaskBits) = avgChunkSize*/
        numOfMaskBits = 1;
        while ((avgChunkSize >> numOfMaskBits) != 1) {

            numOfMaskBits++;
        }
        anchorMask = (1 << numOfMaskBits) - 1;
        /*initialize the value for depolytermining an anchor*/
        anchorValue = 0;
    } else if (ChunkerType == CHUNKER_FIX_SIZE_TYPE) {

        avgChunkSize = (int)config.getAverageChunkSize();
        minChunkSize = (int)config.getMinChunkSize();
        maxChunkSize = (int)config.getMaxChunkSize();
        ReadSize = config.getReadSize();
        ReadSize = ReadSize * 1024 * 1024;
        waitingForChunkingBuffer = new u_char[ReadSize];
        chunkBuffer = new u_char[avgChunkSize];

        if (waitingForChunkingBuffer == NULL || chunkBuffer == NULL) {
            cerr << "Chunker : Memory Error" << endl;
            exit(1);
        }
        if (minChunkSize != avgChunkSize || maxChunkSize != avgChunkSize) {
            cerr << "Chunker : Error: minChunkSize and maxChunkSize should be same "
                    "in fixed size mode!"
                 << endl;
            exit(1);
        }
        if (ReadSize % avgChunkSize != 0) {
            cerr << "Chunker : Setting fixed size chunking error : ReadSize not "
                    "compat with average chunk size"
                 << endl;
        }
    }
}

bool Chunker::chunking() {
//    /*fixed-size Chunker*/
//    if (ChunkerType == CHUNKER_FIX_SIZE_TYPE) {
//        printf("error\n");
////        fixSizeChunking();
//    }
    /*variable-size Chunker*/
    if (ChunkerType == CHUNKER_VAR_SIZE_TYPE) {
        varSizeChunking();
    }
    return true;
}



void Chunker::varSizeChunking() {
    RecipeList_t recipeList;
    Recipe_t fileRecipe;

    gettimeofday(&timestartChunker, NULL);
    uint16_t winFp;
    uint64_t chunkBufferCnt = 0, chunkIDCnt = 0;
    ifstream &fin = getChunkingFile();
    uint64_t fileSize = 0;
    u_char hash[CHUNK_HASH_SIZE];
    /*start chunking*/
    while (true) {
        memset((char *)waitingForChunkingBuffer, 0, sizeof(unsigned char) * ReadSize);
        fin.read((char *)waitingForChunkingBuffer, sizeof(unsigned char) * ReadSize);
        int len = fin.gcount();
        fileSize += len;
        for (int i = 0; i < len; i++) {

            chunkBuffer[chunkBufferCnt] = waitingForChunkingBuffer[i];

            /*full fill sliding window*/
            if (chunkBufferCnt < slidingWinSize) {
                winFp = winFp + (chunkBuffer[chunkBufferCnt] * powerLUT[slidingWinSize - chunkBufferCnt - 1]) &
                        polyMOD; // Refer to doc/Chunking.md hash function:RabinChunker
                chunkBufferCnt++;
                continue;
            }
            winFp &= (polyMOD);

            /*slide window*/
            unsigned short int v = chunkBuffer[chunkBufferCnt - slidingWinSize]; // queue
            winFp = ((winFp + removeLUT[v]) * polyBase + chunkBuffer[chunkBufferCnt]) &
                    polyMOD; // remove queue front and add queue tail
            chunkBufferCnt++;

            /*chunk's size less than minChunkSize*/
            if (chunkBufferCnt < minChunkSize) continue;

            /*find chunk pattern*/
            if ((winFp & anchorMask) == anchorValue) {

                if (!cryptoObj->generateHash(chunkBuffer, chunkBufferCnt, hash)) {
                    cerr << "Chunker : average size chunking compute hash error" << endl;
                    return;
                }

                Data_t tempChunk;
                tempChunk.chunk.ID = chunkIDCnt;
                tempChunk.chunk.logicDataSize = chunkBufferCnt;
                memcpy(tempChunk.chunk.logicData, chunkBuffer, chunkBufferCnt);
                memcpy(tempChunk.chunk.chunkHash, hash, CHUNK_HASH_SIZE);
                tempChunk.dataType = DATA_TYPE_CHUNK;

                chunkcount2++;
                if(chunkcount2==10){
                    printf("abc\n");
                }
                RecipeEntry_t newRecipeEntry;
                newRecipeEntry.chunkID = tempChunk.chunk.ID;
                newRecipeEntry.chunkSize = tempChunk.chunk.logicDataSize;
                memcpy(newRecipeEntry.chunkHash, tempChunk.chunk.chunkHash, CHUNK_HASH_SIZE);
                memcpy(newRecipeEntry.chunkKey, tempChunk.chunk.encryptKey, CHUNK_ENCRYPT_KEY_SIZE);
                recipeList.push_back(newRecipeEntry);
                std::string tmpdata;
                if (fp2ChunkDB.query((char *)hash, tmpdata)) {
                    ;
                } else {
                    if (!storageObj_->saveChunk((char *)hash, (char *)tempChunk.chunk.logicData, tempChunk.chunk.logicDataSize)) {
                        cerr << "DedupCore : dedup stage 2 report error" << endl;
                        return ;
                    }
                }

//                if (!insertMQToKeyClient(tempChunk)) {
//                    cerr << "Chunker : error insert chunk to keyClient MQ for chunk ID = " << tempChunk.chunk.ID
//                         << endl;
//                    return;
//                }
                chunkIDCnt++;
                chunkBufferCnt = winFp = 0;
            }

            /*chunk's size exceed maxChunkSize*/
            if (chunkBufferCnt >= maxChunkSize) {
                if (!cryptoObj->generateHash(chunkBuffer, chunkBufferCnt, hash)) {
                    cerr << "Chunker : average size chunking compute hash error" << endl;
                    return;
                }

                Data_t tempChunk;
                tempChunk.chunk.ID = chunkIDCnt;
                tempChunk.chunk.logicDataSize = chunkBufferCnt;
                memcpy(tempChunk.chunk.logicData, chunkBuffer, chunkBufferCnt);
                memcpy(tempChunk.chunk.chunkHash, hash, CHUNK_HASH_SIZE);
                tempChunk.dataType = DATA_TYPE_CHUNK;

                RecipeEntry_t newRecipeEntry;
                newRecipeEntry.chunkID = tempChunk.chunk.ID;
                newRecipeEntry.chunkSize = tempChunk.chunk.logicDataSize;
                memcpy(newRecipeEntry.chunkHash, tempChunk.chunk.chunkHash, CHUNK_HASH_SIZE);
                memcpy(newRecipeEntry.chunkKey, tempChunk.chunk.encryptKey, CHUNK_ENCRYPT_KEY_SIZE);
                recipeList.push_back(newRecipeEntry);
                std::string tmpdata;
                if (fp2ChunkDB.query((char *)hash, tmpdata)) {
                    ;
                } else {
                    if (!storageObj_->saveChunk((char *)hash, (char *)tempChunk.chunk.logicData, tempChunk.chunk.logicDataSize)) {
                        cerr << "DedupCore : dedup stage 2 report error" << endl;
                        return ;
                    }
                }

//                if (!insertMQToKeyClient(tempChunk)) {
//                    cerr << "Chunker : error insert chunk to keyClient MQ for chunk ID = " << tempChunk.chunk.ID
//                         << endl;
//                    return;
//                }
                chunkIDCnt++;
                chunkBufferCnt = winFp = 0;
            }
        }
        if (fin.eof()) {
            break;
        }
    }

    /*add final chunk*/
    if (chunkBufferCnt != 0) {

        if (!cryptoObj->generateHash(chunkBuffer, chunkBufferCnt, hash)) {
            cerr << "Chunker : average size chunking compute hash error" << endl;
            return;
        }

        Data_t tempChunk;
        tempChunk.chunk.ID = chunkIDCnt;
        tempChunk.chunk.logicDataSize = chunkBufferCnt;
        memcpy(tempChunk.chunk.logicData, chunkBuffer, chunkBufferCnt);
        memcpy(tempChunk.chunk.chunkHash, hash, CHUNK_HASH_SIZE);
        tempChunk.dataType = DATA_TYPE_CHUNK;

        RecipeEntry_t newRecipeEntry;
        newRecipeEntry.chunkID = tempChunk.chunk.ID;
        newRecipeEntry.chunkSize = tempChunk.chunk.logicDataSize;
        memcpy(newRecipeEntry.chunkHash, tempChunk.chunk.chunkHash, CHUNK_HASH_SIZE);
        memcpy(newRecipeEntry.chunkKey, tempChunk.chunk.encryptKey, CHUNK_ENCRYPT_KEY_SIZE);
        recipeList.push_back(newRecipeEntry);
        std::string tmpdata;
        if (fp2ChunkDB.query((char *)hash, tmpdata)) {
            ;
        } else {
            if (!storageObj_->saveChunk((char *)hash, (char *)tempChunk.chunk.logicData, tempChunk.chunk.logicDataSize)) {
                cerr << "DedupCore : dedup stage 2 report error" << endl;
                return ;
            }
        }

//        if (!insertMQToKeyClient(tempChunk)) {
//            cerr << "Chunker : error insert chunk to keyClient MQ for chunk ID = " << tempChunk.chunk.ID << endl;
//            return;
//        }
        chunkIDCnt++;
        chunkBufferCnt = winFp = 0;
    }
    recipe.recipe.fileRecipeHead.totalChunkNumber = chunkIDCnt;
    recipe.recipe.keyRecipeHead.totalChunkKeyNumber = chunkIDCnt;
    recipe.recipe.fileRecipeHead.fileSize = fileSize;
    recipe.recipe.keyRecipeHead.fileSize = recipe.recipe.fileRecipeHead.fileSize;
    recipe.dataType = DATA_TYPE_RECIPE;

    if (!storageObj_->checkRecipeStatus(recipe.recipe, recipeList)) {
        cerr << "StorageCore : verify Recipe fail, send resend flag" << endl;
    } else {
        cerr << "StorageCore : verify Recipe succes" << endl;
    }


//    if (!insertMQToKeyClient(recipe)) {
//        cerr << "Chunker : error insert recipe head to keyClient MQ" << endl;
//        return;
//    }
//    if (setJobDoneFlag() == false) {
//        cerr << "Chunker: set chunking done flag error" << endl;
//        return;
//    }

    cout << "Chunker : variable size chunking over:\nTotal file size = " << recipe.recipe.fileRecipeHead.fileSize
         << "; Total chunk number = " << recipe.recipe.fileRecipeHead.totalChunkNumber << endl;
    gettimeofday(&timeendChunker, NULL);
    long diff =
            1000000 * (timeendChunker.tv_sec - timestartChunker.tv_sec) + timeendChunker.tv_usec - timestartChunker.tv_usec;
    double second = diff / 1000000.0;
    cout << "Chunker : total work time is " << diff << " us = " << second << " s" << endl;
    return;
}


//
//bool Chunker::insertMQToDataBase(Data_t &newData) {
//    return dataSRObj->insertMQFromChunker(newData);
//}

//bool Chunker::setJobDoneFlag() {
//    return keyClientObj->editJobDoneFlag();
//}
