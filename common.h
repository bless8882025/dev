#ifndef COMMON_H
#define COMMON_H

#include <cstdint>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <iostream>
#include <vector>
#include <pthread.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dlfcn.h>
#include <errno.h>
#include <algorithm>
#include <endian.h>   // для htobe64, be64toh

const uint8_t CMD_GET = 'G';
const uint8_t CMD_PUT = 'P';

bool sendAll(int sockfd, const void* buffer, size_t len);
bool recvAll(int sockfd, void* buffer, size_t len);

#endif
