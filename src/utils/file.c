#include "file.h"
#include "../common.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#define MAX_SOURCE_SIZE (64 * 1024 * 1024)

char *read_source_file(const char *path) {
    assert(path);
    FILE *file = fopen(path, "rb");
    if (!file) {
        perror("Error opening source file");
        return NULL;
    }

    if (fseek(file, 0L, SEEK_END) != 0) {
        perror("Error seeking in source file");
        fclose(file);
        return NULL;
    }

    long file_size = ftell(file);
    if (file_size < 0) {
        perror("Could not determine file size");
        fclose(file);
        return NULL;
    }

    if (file_size > MAX_SOURCE_SIZE) {
        fprintf(stderr, "Error: file '%s' is too large (%ld bytes)\n", path, file_size);
        fclose(file);
        return NULL;
    }

    rewind(file);

    size_t size = (size_t)file_size;
    char *buffer = xmalloc(size + 1);

    size_t bytes_read = fread(buffer, 1, size, file);
    if (bytes_read < size) {
        fprintf(stderr, "Error: failed to completely read file '%s'\n", path);
        free(buffer);
        fclose(file);
        return NULL;
    }

    buffer[bytes_read] = '\0';
    fclose(file);
    return buffer;
}
