#ifndef FILEOPS_H
#define FILEOPS_H

#include <cstddef>
#include <string>

#ifdef __cplusplus
extern "C" {
#endif

int file_open_read(const char* path);
int file_open_write(const char* path);
int file_read_chunk(int fd, void* buffer, size_t size);
int file_write_chunk(int fd, const void* buffer, size_t size);
void file_close(int fd);
long long file_get_size(int fd);

#ifdef __cplusplus
}
#endif

#endif
