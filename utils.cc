#include <stdlib.h>
#include <stdio.h>

#include "utils.h"
#include "fs.h"


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

uint64_t getLinksLen(struct inode* node){
    int len= 0;
    while(node->links[len++] != 0 );
    return len;
}

uint64_t findFile(const struct superblock* sb ,const char* fname){
    struct inode* node = (struct inode*) malloc(sizeof(sb->blksz));
    SEEK_READ(sb, sb->root, node);
    if()
    
}