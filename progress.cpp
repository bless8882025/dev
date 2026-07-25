#include "progress.h"
#include <cstdio>

extern "C" {

void print_progress(size_t sent, size_t total) {
    if (total == 0) {
        printf("Progress: %zu bytes sent / 0 total (N/A)\n", sent);
        return;
    }
    int percent = (int)((double)sent / total * 100);
    printf("\rProgress: %zu / %zu bytes (%d%%)", sent, total, percent);
    fflush(stdout);
}

}
