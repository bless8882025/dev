#ifndef PROGRESS_H
#define PROGRESS_H

#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

void print_progress(size_t sent, size_t total);

#ifdef __cplusplus
}
#endif

#endif
