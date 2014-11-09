#include <stdlib.h>
#include <stdio.h>

#include "utils.h"


int getFileSize(char* fname) {
    FILE *fd = fopen(fname, "r");
    fseek(fd, 0, SEEK_END);
    int size = ftell(fd);
    rewind(fd);
    fclose(fd);
    return size;
}