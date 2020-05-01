cmake_minimum_required(VERSION 3.15)
project(chunk_new)

#set(CMAKE_CXX_STANDARD 14)
#set( OPENSSL_ROOT_DIR "/usr/local/opt/openssl")
#find_package(OpenSSL REQUIRED)
#if ( OPENSSL_FOUND )
#    message(STATUS "OpenSSL Found: ${OPENSSL_VERSION}")
#    message(STATUS "OpenSSL Include: ${OPENSSL_INCLUDE_DIR}")
#    message(STATUS "OpenSSL Libraries: ${OPENSSL_LIBRARIES}")
#endif()
#
#set(OPENSSL_LIBRARY_OBJ ssl crypto)
#
#
#include_directories(${OPENSSL_INCLUDE_PATH} )
#link_directories(${OPENSSL_LIBRARY_PATH} )

#set(OPENSSL_LIBRARY_OBJ ssl crypto)

#link_libraries(ssl crypto)

#find_package(OPENSSL REQUIRED)


set(OPENSSL_INCLUDE_DIR ../openssl/include)
set(OPENSSL_LIBRARY_PATH ../openssl)

include_directories(${OPENSSL_INCLUDE_DIR})
link_directories(${OPENSSL_LIBRARY_PATH})
link_libraries(ssl crypto)

#IF (APPLE)
#    # Fix linking on 10.14+. See https://stackoverflow.com/questions/54068035
#    LINK_DIRECTORIES(/usr/local/lib)
#
#    # Add to enable openssl source searching for cmake
#    set(OPENSSL_INCLUDE_DIR, /usr/local/opt/openssl@1.1/include)
#    include_directories(${OPENSSL_INCLUDE_DIR})
#ENDIF ()


find_package(Boost  REQUIRED COMPONENTS thread system serialization)
if(Boost_FOUND)
    include_directories(${Boost_INCLUDE_DIRS})
    link_directories(${BOOST_LIBRARY_DIR})
endif()
link_libraries(boost_thread pthread  boost_system boost_serialization)

add_executable(chunk_new configure.h configure.cpp main.cpp Chunker.cpp Chunker.h keyClient.cpp keyClient.h dataStructure.h cryptoPrimitive.cpp cryptoPrimitive.h stdc++.h messageQueue.h sender.cpp sender.cpp sender.h)

target_link_libraries(chunk_new )