cmake_minimum_required(VERSION 3.15)
project(chunk_new)



set(OPENSSL_INCLUDE_DIR ../openssl/include)
set(OPENSSL_LIBRARY_PATH ../openssl)

set(LEVELDB_INCLUDE_DIR ../leveldb/include)
set(LEVELDB_LIBRARY_PATH ../leveldb/build)

include_directories(${OPENSSL_INCLUDE_DIR})
link_directories(${OPENSSL_LIBRARY_PATH})
include_directories(${LEVELDB_INCLUDE_DIR})
link_directories(${LEVELDB_LIBRARY_PATH})
link_libraries(ssl crypto)
link_libraries(pthread leveldb snappy)



find_package(Boost  REQUIRED COMPONENTS thread system serialization)
if(Boost_FOUND)
    include_directories(${Boost_INCLUDE_DIRS})
    link_directories(${BOOST_LIBRARY_DIR})
endif()
link_libraries(boost_thread pthread  boost_system boost_serialization)

add_executable(chunk_new configure.hpp configure.cpp protocol.hpp main.cpp Chunker.cpp Chunker.hpp keyClient.cpp keyClient.hpp dataStructure.hpp cryptoPrimitive.cpp cryptoPrimitive.hpp stdc++.h messageQueue.hpp sender.cpp sender.hpp database.hpp database.cpp
        dataSR.hpp dataSR.cpp storageCore.hpp storageCore.cpp recvDecode.hpp recvDecode.cpp retriever.hpp retriever.cpp)

target_link_libraries(chunk_new )