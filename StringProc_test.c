#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

#include "StringProc.h"

int main(int argc, char** argv) {
    char* fname = argv[1];
    char**fileParts = NULL;
    int len = 0;
    printf("Testing getFileParts\n");
    fileParts = getFileParts(fname, &len);
    printf("len is :[%d]\n", len);
    for (int i = 0; i < len; i++) {
        printf("part_%d[%s]\n", i, fileParts[i]);
    }
    printf("Testing getParentNodeName\n");
    char* parentName = getParentNodeName(fname);
    printf("parent[%s]\n", parentName);

    return EXIT_SUCCESS;
}
