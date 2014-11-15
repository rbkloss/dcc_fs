#include <stdlib.h>
#include <stdio.h>

#include "utils.h"
#include "fs.h"
#include "StringProc.h"


int getFileSize(const char* fname) {
    FILE *fd = fopen(fname, "r");
    fseek(fd, 0, SEEK_END);
    int size = ftell(fd);
    rewind(fd);
    fclose(fd);
    return size;
}

int getINodeLinksCap(struct superblock* sb){
    int ans = sizeof(struct inode) - sb->blksz;
    return ans;
}

int getLinksLen(struct inode* node){
    int len= 0;
    while(node->links[len++] != 0 );
    return len;
}

uint64_t findFile(const struct superblock* sb ,const char* fname){
    struct inode* node = (struct inode*) malloc(sizeof(sb->blksz));
    int len = 0;
    char** fileParts = getFileParts(fname, &len);
    
    SEEK_READ(sb, sb->root, node);
    int it = 0;
    while(it < len){
        int nEnts = getLinksLen(node);
        for(int i=0; i < nEnts; i++){
            
        }
    }
    
}