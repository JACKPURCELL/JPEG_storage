//
// Created by a on 11/17/18.
//

#include "configure.hpp"

Configure::~Configure() {}
Configure::Configure() {}
Configure::Configure(std::string path)
{
    this->readConf(path);
}

void Configure::readConf(std::string path)
{
    using namespace boost;
    using namespace boost::property_tree;
    ptree root;
    read_json<ptree>(path, root);

    //Chunker Configure
    _runningType = root.get<uint64_t>("ChunkerConfig._runningType");
    _chunkingType = root.get<uint64_t>("ChunkerConfig._chunkingType");
    _maxChunkSize = root.get<uint64_t>("ChunkerConfig._maxChunkSize");
    _minChunkSize = root.get<uint64_t>("ChunkerConfig._minChunkSize");
    _slidingWinSize = root.get<uint64_t>("ChunkerConfig._slidingWinSize");
    _segmentSize = root.get<uint64_t>("ChunkerConfig._segmentSize");
    _averageChunkSize = root.get<uint64_t>("ChunkerConfig._avgChunkSize");
    _ReadSize = root.get<uint64_t>("ChunkerConfig._ReadSize");

    //Key Server Configure
    _keyServerNumber = root.get<uint64_t>("KeyServerConfig._keyServerNumber");
    _keyBatchSize = root.get<uint64_t>("KeyServerConfig._keyBatchSize");
    _keyServerIP.clear();
    for (ptree::value_type& it : root.get_child("KeyServerConfig._keyServerIP")) {
        _keyServerIP.push_back(it.second.data());
    }
    _keyServerPort.clear();
    for (ptree::value_type& it : root.get_child("KeyServerConfig._keyServerPort")) {
        _keyServerPort.push_back(it.second.get_value<int>());
    }
    _sketchTableWidth = root.get<uint64_t>("KeyServerConfig._sketchTableWidth");
    _optimalSolverComputeItemNumberThreshold = root.get<int>("KeyServerConfig._optimalSolverComputeItemNumberThreshold");
    _storageBlowPercent = root.get<double>("KeyServerConfig._storageBlowPercent");
    //SP Configure
    _storageServerNumber = root.get<uint64_t>("SPConfig._storageServerNumber");
    _maxContainerSize = root.get<uint64_t>("SPConfig._maxContainerSize");
    _storageServerIP.clear();
    for (ptree::value_type& it : root.get_child("SPConfig._storageServerIP")) {
        _storageServerIP.push_back(it.second.data());
    }

    _storageServerPort.clear();
    for (ptree::value_type& it : root.get_child("SPConfig._storageServerPort")) {
        _storageServerPort.push_back(it.second.get_value<int>());
    }

    _Data_t_MQSize = root.get<int>("messageQueue._Data_t_MQSize");
    _RetrieverData_t_MQSize = root.get<int>("messageQueue._RetrieverData_t_MQSize");
    _StorageData_t_MQSize = root.get<int>("messageQueue._StorageData_t_MQSize");

    //multi thread settings;
    _encodeThreadLimit = root.get<int>("mutiThread._encodeThreadLimit");
    _keyClientThreadLimit = root.get<int>("mutiThread._keyClientThreadLimit");
    _keyServerThreadLimit = root.get<int>("mutiThread._keyServerThreadLimit");
    _senderThreadLimit = root.get<int>("mutiThread._senderThreadLimit");
    _recvDecodeThreadLimit = root.get<int>("mutiThread._recvDecodeThreadLimit");
    _dataSRThreadLimit = root.get<int>("mutiThread._dataSRThreadLimit");
    _retriverThreadLimit = root.get<int>("mutiThread._retriverThreadLimit");
    _dedupCoreThreadLimit = root.get<int>("mutiThread._dedupCoreThreadLimit");
    _storageCoreThreadLimit = root.get<int>("mutiThread._storageCoreThreadLimit");

    //server Configure
    _RecipeRootPath = root.get<std::string>("server._RecipeRootPath");
    _containerRootPath = root.get<std::string>("server._containerRootPath");
    _fp2ChunkDBName = root.get<std::string>("server._fp2ChunkDBName");
    _fp2MetaDBame = root.get<std::string>("server._fp2MetaDBame");

    //client Configure
    _clientID = root.get<int>("client._clientID");
    _sendChunkBatchSize = root.get<int>("client._sendChunkBatchSize");
    _sendRecipeBatchSize = root.get<int>("client._sendRecipeBatchSize");
    _sendShortHashMaskBitNumber = root.get<int>("client._sendShortHashMaskBitNumber");
}

uint64_t Configure::getRunningType()
{

    return _runningType;
}

// chunking settings
uint64_t Configure::getChunkingType()
{

    return _chunkingType;
}

uint64_t Configure::getMaxChunkSize()
{

    return _maxChunkSize;
}

uint64_t Configure::getMinChunkSize()
{

    return _minChunkSize;
}

uint64_t Configure::getAverageChunkSize()
{

    return _averageChunkSize;
}

uint64_t Configure::getSlidingWinSize()
{

    return _slidingWinSize;
}

uint64_t Configure::getSegmentSize()
{

    return _segmentSize;
}

uint64_t Configure::getReadSize()
{
    return _ReadSize;
}

// key management settings
uint64_t Configure::getKeyServerNumber()
{

    return _keyServerNumber;
}

int Configure::getKeyBatchSize()
{

    return _keyBatchSize;
}

uint64_t Configure::getSketchTableWidth()
{
    return _sketchTableWidth;
}
double Configure::getStorageBlowPercent()
{
    return _storageBlowPercent;
}

/*
std::vector<std::string> Configure::getkeyServerIP() {

    return _keyServerIP;
}

std::vector<int> Configure::getKeyServerPort() {

    return _keyServerPort;
}

*/
std::string Configure::getKeyServerIP()
{
    return _keyServerIP[0];
}

int Configure::getKeyServerPort()
{
    return _keyServerPort[0];
}

int Configure::getOptimalSolverComputeItemNumberThreshold()
{
    return _optimalSolverComputeItemNumberThreshold;
}

// message queue size setting
int Configure::get_Data_t_MQSize()
{
    return _Data_t_MQSize;
}

int Configure::get_RetrieverData_t_MQSize()
{
    return _RetrieverData_t_MQSize;
}
int Configure::get_StorageData_t_MQSize()
{
    return _StorageData_t_MQSize;
}

//muti thread settings
int Configure::getEncoderThreadLimit()
{
    return _encodeThreadLimit;
}

int Configure::getKeyClientThreadLimit()
{
    return _keyClientThreadLimit;
}

int Configure::getKeyServerThreadLimit()
{
    return _keyServerThreadLimit;
}

// storage management settings
uint64_t Configure::getStorageServerNumber()
{

    return _storageServerNumber;
}

std::string Configure::getStorageServerIP()
{

    return _storageServerIP[0];
}
/*
std::vector<std::string> Configure::getStorageServerIP() {

    return _storageServerIP;
}*/

int Configure::getStorageServerPort()
{

    return _storageServerPort[0];
}

/*
std::vector<int> Configure::getStorageServerPort() {

    return _storageServerPort;
}*/

uint64_t Configure::getMaxContainerSize()
{

    return _maxContainerSize;
}

// client settings
int Configure::getClientID()
{
    return _clientID;
}

int Configure::getSendChunkBatchSize()
{
    return _sendChunkBatchSize;
}

std::string Configure::getRecipeRootPath()
{
    return _RecipeRootPath;
}

std::string Configure::getContainerRootPath()
{
    return _containerRootPath;
}

std::string Configure::getFp2ChunkDBName()
{
    return _fp2ChunkDBName;
}

std::string Configure::getFp2MetaDBame()
{
    return _fp2MetaDBame;
}

int Configure::getSenderThreadLimit()
{
    return _senderThreadLimit;
}

int Configure::getRecvDecodeThreadLimit()
{
    return _recvDecodeThreadLimit;
}

int Configure::getDataSRThreadLimit()
{
    return _dataSRThreadLimit;
}

int Configure::getRetriverThreadLimit()
{
    return _retriverThreadLimit;
}

int Configure::getDedupCoreThreadLimit()
{
    return _dedupCoreThreadLimit;
}

int Configure::getStorageCoreThreadLimit()
{
    return _storageCoreThreadLimit;
}

int Configure::getSendRecipeBatchSize()
{
    return _sendRecipeBatchSize;
}

int Configure::getSendShortHashMaskBitNumber()
{
    return _sendShortHashMaskBitNumber;
}
