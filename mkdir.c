#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <errno.h>
#include <fcntl.h>

#include <assert.h>

#include "fs.h"
#include "utils.h"
#include "StringProc.h"

#define SB_SIZE 52

#define notvalid -1
#define success 0

int fs_mkdir(struct superblock *sb, const char *dname){

  if (!sb->freeblks){
    // disk is already full
    errno = ENOSPC;
    return notvalid;
  }

  int exists = 0;

  uint64_t fileBlock = findFile (sb, dname, &exists);

  if (exists){
    // dir already exist;
    errno = EEXIST;
    return notvalid;
  }

  struct inode *father = (struct inode*) malloc (sb->blksz);
  struct inone *folder = (struct inode*) malloc (sb->blksz);

  if (!exists){
    SEEK_READ (sb, fileBlock, father);
    
  }
}
