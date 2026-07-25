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

const uint8_t CMD_GET = 'G';
const uint8_t CMD_PUT = 'P';

static inline uint64_t htobe64(uint64_t x) {
    return ((uint64_t)htonl(x >> 32) << 32) | htonl(x & 0xffffffff);
}
static inline uint64_t be64toh(uint64_t x) {
    return ((uint64_t)ntohl(x >> 32) << 32) | ntohl(x & 0xffffffff);
}

bool sendAll(int sockfd, const void* buffer, size_t len) {
    const char* buf = (const char*)buffer;
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = send(sockfd, buf + sent, len - sent, 0);
        if (n <= 0) return false;
        sent += n;
    }
    return true;
}

bool recvAll(int sockfd, void* buffer, size_t len) {
    char* buf = (char*)buffer;
    size_t received = 0;
    while (received < len) {
        ssize_t n = recv(sockfd, buf + received, len - received, 0);
        if (n <= 0) return false;
        received += n;
    }
    return true;
}

#endif
