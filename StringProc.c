#include "StringProc.h"

char** getFileParts(const char* fname) {
    char* bar_p;
    char* name = fname;
    int len = 0;
    while ((bar_p = strstr(fname, "/")) != NULL) {
        len++;
    }
    char** ans = malloc(sizeof (char*) * len);
    
    for (int i = 0; i < len; i++) {
        int next_bar = strspn(name + 1, '/');
        char* part = malloc(sizeof (next_bar));
        ans[i] = part;        
        name = name + next_bar;
    }
    return ans;
}

char* getParentNodeName(const char*fname) {
    char* bar_p;
    char* last_bar_p;
    int len = 0;
    last_bar_p = strstr(fname, "/");
    len++;
    bar_p = strstr(fname, "/");
    if (bar_p) {
        len = 2;
    } else {
        return NULL;
    }
    while ((bar_p = strstr(fname, "/")) != NULL) {
        last_bar_p = bar_p;
        len++;
    }
    int parentLen = strlen(fname) - strlen(strchr(last_bar_p, '/'));
    char* parentName = malloc(parentLen + 1);
    strncpy(parentName, last_bar_p, parentLen);
    return parentName;
}