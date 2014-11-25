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
    // disk is full
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
  struct nodeinfo *n_info = (struct nodeinfo*) malloc (sb->blksz);

  uint64_t folder_block = fs_get_block (sb);
  uint64_t nodeinfo_block = fs_get_block (sb);


  /* inode properties for a folder */ 

  /* =mode indicates that the inode is a directory */
  folder->mode = IMDIR;
  /* =parent points to the block that contains this inode */
  folder->parent = folder_block;
  /* start the =next value with 0 */
  folder->next = 0;
  /* =meta value must point to the nodeinfo block */
  folder->meta = nodeinf_block;


  /* nodeinfo properties */

  /* =size contains the number of files in the directory*/
  n_info->size = 0;
  /* =name contains the full path to the directory
   * i.e. /home/user/dir1/folder
   */
  n_info->name = dname;


  if (!exists){
    /* Read the inode stored in file */
    SEEK_READ (sb, fileBlock, father);

    

    SEEK_WRITE (sb, folder_block, folder);
    SEEK_WRITE (sb, nodeinfo_block, n_info);
    
  }
}
