#include "common.h"
#include "fileops.h"
#include <iostream>
#include <string>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <signal.h>
#include <unistd.h>

volatile sig_atomic_t running = 1;
int sockfd = -1;

void signal_handler(int sig) {
    if (sig == SIGINT) {
        running = 0;
        if (sockfd >= 0) {
            close(sockfd);
            sockfd = -1;
        }
    }
}

bool parse_ip_path(const std::string& str, std::string& ip, std::string& path) {
    size_t pos = str.find(':');
    if (pos == std::string::npos) return false;
    ip = str.substr(0, pos);
    path = str.substr(pos + 1);
    struct sockaddr_in sa;
    return inet_pton(AF_INET, ip.c_str(), &(sa.sin_addr)) == 1;
}

int main(int argc, char* argv[]) {
    std::string src, dst;
    int port = 8080;
    size_t chunk_size = 1024;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            port = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-chunk") == 0 && i + 1 < argc) {
            chunk_size = atoi(argv[++i]);
            if (chunk_size == 0) chunk_size = 1;
        } else if (strcmp(argv[i], "-src") == 0 && i + 1 < argc) {
            src = argv[++i];
        } else if (strcmp(argv[i], "-dst") == 0 && i + 1 < argc) {
            dst = argv[++i];
        } else {
            std::cerr << "Unknown argument: " << argv[i] << std::endl;
            return 1;
        }
    }

    if (src.empty() || dst.empty()) {
        std::cerr << "Usage: " << argv[0] << " -src <source> -dst <destination> [-p port] [-chunk size]\n";
        return 1;
    }

    bool is_get = false;
    std::string server_ip, server_path, local_path;
    if (parse_ip_path(src, server_ip, server_path)) {
        is_get = true;
        local_path = dst;
    } else if (parse_ip_path(dst, server_ip, server_path)) {
        is_get = false;
        local_path = src;
    } else {
        std::cerr << "Error: neither src nor dst contains IP:path.\n";
        return 1;
    }

    signal(SIGINT, signal_handler);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return 1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, server_ip.c_str(), &server_addr.sin_addr) <= 0) {
        perror("inet_pton");
        close(sockfd);
        return 1;
    }

    if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        close(sockfd);
        return 1;
    }

    void* progress_lib = dlopen("./libprogress.so", RTLD_LAZY);
    typedef void (*print_progress_t)(size_t, size_t);
    print_progress_t print_progress = nullptr;
    if (progress_lib) {
        print_progress = (print_progress_t)dlsym(progress_lib, "print_progress");
        if (!print_progress) {
            std::cerr << "Cannot find print_progress in library.\n";
            dlclose(progress_lib);
            progress_lib = nullptr;
        }
    }

    auto fallback_print = [](size_t sent, size_t total) {
        printf("\rProgress: %zu / %zu bytes", sent, total);
        fflush(stdout);
    };

    uint8_t cmd = is_get ? CMD_GET : CMD_PUT;
    uint32_t chunk_net = htonl((uint32_t)chunk_size);
    uint16_t path_len_net = htons((uint16_t)server_path.size());
    if (!sendAll(sockfd, &cmd, 1) ||
        !sendAll(sockfd, &chunk_net, 4) ||
        !sendAll(sockfd, &path_len_net, 2) ||
        !sendAll(sockfd, server_path.c_str(), server_path.size())) {
        std::cerr << "Failed to send request.\n";
        close(sockfd);
        return 1;
    }

    int local_fd = -1;
    if (is_get) {
        local_fd = file_open_write(local_path.c_str());
    } else {
        local_fd = file_open_read(local_path.c_str());
        if (local_fd >= 0) {
            long long size = file_get_size(local_fd);
            if (size < 0) {
                std::cerr << "Cannot get size of local file.\n";
                close(local_fd);
                close(sockfd);
                return 1;
            }
            uint64_t size_net = htobe64((uint64_t)size);
            if (!sendAll(sockfd, &size_net, 8)) {
                std::cerr << "Failed to send file size.\n";
                close(local_fd);
                close(sockfd);
                return 1;
            }
        }
    }

    if (local_fd < 0) {
        std::cerr << "Cannot open local file: " << local_path << std::endl;
        close(sockfd);
        return 1;
    }

    uint64_t total_bytes = 0;
    uint64_t transferred = 0;

    if (is_get) {
        uint64_t size_net;
        if (!recvAll(sockfd, &size_net, 8)) {
            std::cerr << "Failed to receive file size.\n";
            file_close(local_fd);
            close(sockfd);
            return 1;
        }
        total_bytes = be64toh(size_net);
        if (total_bytes == 0) {
            std::cerr << "Server file not found or empty.\n";
            file_close(local_fd);
            close(sockfd);
            return 1;
        }

        std::vector<char> buffer(chunk_size);
        while (transferred < total_bytes && running) {
            size_t to_read = chunk_size;
            if (transferred + to_read > total_bytes)
                to_read = total_bytes - transferred;
            if (!recvAll(sockfd, buffer.data(), to_read)) {
                std::cerr << "Error receiving data.\n";
                break;
            }
            ssize_t n = file_write_chunk(local_fd, buffer.data(), to_read);
            if (n != (ssize_t)to_read) {
                std::cerr << "Error writing to local file.\n";
                break;
            }
            transferred += to_read;
            if (print_progress) print_progress(transferred, total_bytes);
            else fallback_print(transferred, total_bytes);
        }

        if (transferred < total_bytes) {
            file_close(local_fd);
            unlink(local_path.c_str());
            std::cerr << "\nTransfer incomplete, file removed.\n";
        } else {
            file_close(local_fd);
            std::cout << std::endl << "File received successfully." << std::endl;
        }
    } else {
        long long size = file_get_size(local_fd);
        if (size < 0) {
            std::cerr << "Cannot get size.\n";
            file_close(local_fd);
            close(sockfd);
            return 1;
        }
        total_bytes = (uint64_t)size;
        std::vector<char> buffer(chunk_size);
        while (transferred < total_bytes && running) {
            size_t to_read = chunk_size;
            if (transferred + to_read > total_bytes)
                to_read = total_bytes - transferred;
            ssize_t n = file_read_chunk(local_fd, buffer.data(), to_read);
            if (n <= 0) {
                std::cerr << "Error reading local file.\n";
                break;
            }
            if (!sendAll(sockfd, buffer.data(), n)) {
                std::cerr << "Error sending data.\n";
                break;
            }
            transferred += n;
            if (print_progress) print_progress(transferred, total_bytes);
            else fallback_print(transferred, total_bytes);
        }
        file_close(local_fd);
        if (transferred < total_bytes) {
            std::cerr << "\nUpload incomplete.\n";
        } else {
            std::cout << std::endl << "File uploaded successfully." << std::endl;
        }
    }

    close(sockfd);
    if (progress_lib) dlclose(progress_lib);
    return 0;
}
