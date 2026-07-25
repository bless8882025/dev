#include "fileops.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

extern "C" {

int file_open_read(const char* path) {
    return open(path, O_RDONLY);
}

int file_open_write(const char* path) {
    return open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
}

int file_read_chunk(int fd, void* buffer, size_t size) {
    return read(fd, buffer, size);
}

int file_write_chunk(int fd, const void* buffer, size_t size) {
    return write(fd, buffer, size);
}

void file_close(int fd) {
    if (fd >= 0) close(fd);
}

long long file_get_size(int fd) {
    off_t pos = lseek(fd, 0, SEEK_CUR);
    if (pos == -1) return -1;
    off_t size = lseek(fd, 0, SEEK_END);
    if (size == -1) return -1;
    lseek(fd, pos, SEEK_SET);
    return (long long)size;
}

}
