#include "StringProc.h"

char** getFileParts(const char* fname, int* len) {
    char** temp = (char**) malloc(sizeof (char*)*256);
    char* name = NULL, * part = NULL, * pch = NULL;
    name = (char*) malloc(sizeof (char)*(strlen(fname) + 1));
    strcpy(name, fname);
    pch = strtok(name, "/");
    part = (char*) malloc(strlen(pch) + 1);
    strcpy(part, pch);
    temp[0] = part;
    *len = 0;
    int i = 1;
    while (pch != NULL) {
        pch = strtok(NULL, "/");
        if (pch) {
            part = (char*) malloc(strlen(pch) + 1);
            strcpy(part, pch);
            temp[i++] = part;
        }
        (*len)++;
    }

    char** ans = (char**) malloc(sizeof (char*)*(*len));
    for (int i = 0; i < *len; i++) {
        ans[i] = temp[i];
    }
    free(temp);
    free(name);
    return ans;
}

char* getParentNodeName(const char*fname) {
    char* name = NULL, * parent = NULL, * pch = NULL, *old = NULL;
    name = (char*) malloc(sizeof (char)*(strlen(fname) + 1));
    strcpy(name, fname);
    parent = strtok(name, "/");
    pch = strtok(NULL, "/");
    while (pch != NULL) {
        old = pch;
        pch = strtok(NULL, "/");
        if (pch) {
            parent = old;
        }
    }
    char* aux = (char*) malloc(sizeof (char)*(strlen(parent) + 1));
    strcpy(aux, parent);
    free(name);
    return aux;
}

void freeFileParts(char*** fParts, const int len) {
    for (int i = 0; i < len; i++) {
        free((*fParts)[i]);
        (*fParts)[i] = NULL;
    }
    free(*fParts);
    *fParts = NULL;
}
