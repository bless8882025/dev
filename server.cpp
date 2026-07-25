#include "common.h"
#include "fileops.h"
#include <iostream>
#include <vector>
#include <string>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <signal.h>

volatile sig_atomic_t running = 1;
int listen_sock = -1;
std::vector<int> client_socks;
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

void signal_handler(int sig) {
    if (sig == SIGINT) {
        running = 0;
        if (listen_sock >= 0) {
            close(listen_sock);
            listen_sock = -1;
        }
        pthread_mutex_lock(&clients_mutex);
        for (int sock : client_socks) {
            close(sock);
        }
        client_socks.clear();
        pthread_mutex_unlock(&clients_mutex);
    }
}

void* handle_client(void* arg) {
    int client_sock = *(int*)arg;
    delete (int*)arg;

    uint8_t cmd;
    uint32_t chunk_size_net;
    uint16_t path_len_net;
    if (!recvAll(client_sock, &cmd, 1) ||
        !recvAll(client_sock, &chunk_size_net, 4) ||
        !recvAll(client_sock, &path_len_net, 2)) {
        close(client_sock);
        return nullptr;
    }
    uint32_t chunk_size = ntohl(chunk_size_net);
    uint16_t path_len = ntohs(path_len_net);
    std::vector<char> path_buf(path_len + 1, 0);
    if (!recvAll(client_sock, path_buf.data(), path_len)) {
        close(client_sock);
        return nullptr;
    }
    std::string path(path_buf.data());

    if (cmd == CMD_GET) {
        int fd = file_open_read(path.c_str());
        if (fd < 0) {
            uint64_t size_net = 0;
            sendAll(client_sock, &size_net, 8);
            close(client_sock);
            return nullptr;
        }
        long long size = file_get_size(fd);
        if (size < 0) {
            file_close(fd);
            uint64_t size_net = 0;
            sendAll(client_sock, &size_net, 8);
            close(client_sock);
            return nullptr;
        }
        uint64_t size_net = htobe64((uint64_t)size);
        if (!sendAll(client_sock, &size_net, 8)) {
            file_close(fd);
            close(client_sock);
            return nullptr;
        }
        std::vector<char> buffer(chunk_size);
        uint64_t total_sent = 0;
        while (total_sent < (uint64_t)size && running) {
            size_t to_read = chunk_size;
            if (total_sent + to_read > (uint64_t)size)
                to_read = (uint64_t)size - total_sent;
            ssize_t n = file_read_chunk(fd, buffer.data(), to_read);
            if (n <= 0) break;
            if (!sendAll(client_sock, buffer.data(), n)) break;
            total_sent += n;
        }
        file_close(fd);
        close(client_sock);
    } else if (cmd == CMD_PUT) {
        uint64_t size_net;
        if (!recvAll(client_sock, &size_net, 8)) {
            close(client_sock);
            return nullptr;
        }
        uint64_t size = be64toh(size_net);
        int fd = file_open_write(path.c_str());
        if (fd < 0) {
            close(client_sock);
            return nullptr;
        }
        std::vector<char> buffer(chunk_size);
        uint64_t received = 0;
        while (received < size && running) {
            size_t to_read = chunk_size;
            if (received + to_read > size)
                to_read = size - received;
            if (!recvAll(client_sock, buffer.data(), to_read)) break;
            ssize_t n = file_write_chunk(fd, buffer.data(), to_read);
            if (n != (ssize_t)to_read) break;
            received += to_read;
        }
        file_close(fd);
        close(client_sock);
    } else {
        close(client_sock);
    }

    pthread_mutex_lock(&clients_mutex);
    auto it = std::find(client_socks.begin(), client_socks.end(), client_sock);
    if (it != client_socks.end()) client_socks.erase(it);
    pthread_mutex_unlock(&clients_mutex);

    return nullptr;
}

int main(int argc, char* argv[]) {
    int port = 8080;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            port = atoi(argv[++i]);
        }
    }

    signal(SIGINT, signal_handler);

    listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_sock < 0) {
        perror("socket");
        return 1;
    }
    int opt = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(listen_sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(listen_sock);
        return 1;
    }
    if (listen(listen_sock, 10) < 0) {
        perror("listen");
        close(listen_sock);
        return 1;
    }

    std::cout << "Server listening on port " << port << std::endl;

    while (running) {
        struct sockaddr_in client_addr;
        socklen_t len = sizeof(client_addr);
        int client_sock = accept(listen_sock, (struct sockaddr*)&client_addr, &len);
        if (client_sock < 0) {
            if (!running) break;
            perror("accept");
            continue;
        }

        pthread_mutex_lock(&clients_mutex);
        client_socks.push_back(client_sock);
        pthread_mutex_unlock(&clients_mutex);

        int* sock_ptr = new int(client_sock);
        pthread_t tid;
        if (pthread_create(&tid, nullptr, handle_client, sock_ptr) != 0) {
            perror("pthread_create");
            close(client_sock);
            delete sock_ptr;
            pthread_mutex_lock(&clients_mutex);
            client_socks.pop_back();
            pthread_mutex_unlock(&clients_mutex);
        } else {
            pthread_detach(tid);
        }
    }

    if (listen_sock >= 0) close(listen_sock);
    std::cout << "Server shut down." << std::endl;
    return 0;
}
