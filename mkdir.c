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

#define invalid -1
#define success 0

void init_folder_struct (struct inode* folder, uint64_t folder_block, uint64_t nodeinfo_block) {

	 /* inode properties for a folder */ 

  /* =mode indicates that the inode is a directory */
  folder->mode = IMDIR;
  /* =parent points to the block that contains this inode */
  folder->parent = folder_block;
  /* start the =next value with 0 */
  folder->next = 0;
  /* =meta value must point to the nodeinfo block */
  folder->meta = nodeinfo_block;

}

int init_nodeinfo_struct (struct superblock* sb, const char* dname, struct nodeinfo* n_info, uint64_t nodeinfo_block){
	/* nodeinfo properties */

  /* =size contains the number of files in the directory*/
  n_info->size = 0;

  /* =name contains the name of the new folder
   * i.e. /home/user/dir1/folder
	 * =name is equals to folder
   */
	int size = 0;
	char** dname_array = getFileParts (dname, &size);
  int max_filename_size = getFileNameMaxLen (sb);
  int foldername_size = strlen (dname_array[size-1]);
  if (max_filename_size < foldername_size){
    return invalid;
  }
	
  strcpy (n_info->name, dname_array[size-1]);
  freeFileParts (&dname_array, size);
  
  return success;
}

int checkfather_path (struct superblock *sb, const char *dname, struct inode* father){
  
  struct nodeinfo* info_pai = (struct nodeinfo*) malloc (sb->blksz);
  SEEK_READ (sb, father->meta, info_pai);
  
  int size = 0;
  char **dname_array = getFileParts (dname, &size);
  
  int valid = strcmp (dname_array[size-2], info_pai->name);

  freeFileParts (&dname_array, size);
  free (info_pai);
  info_pai = NULL;

  if (valid == 0){
    return success;
  }
  else {
    return invalid;
  }
}


int fs_mkdir(struct superblock *sb, const char *dname){

  if (!sb->freeblks){
    // disk is full
    errno = ENOSPC;
    return invalid;
  }

  int exists = 0;

  uint64_t fileBlock = findFile (sb, dname, &exists);

  if (exists){
    // dir already exist;
    errno = EEXIST;
    return invalid;
  }

  struct inode *father = (struct inode*) malloc (sb->blksz);
  struct inode *folder = (struct inode*) malloc (sb->blksz);
  struct nodeinfo *n_info = (struct nodeinfo*) malloc (sb->blksz);

  uint64_t folder_block = fs_get_block (sb);
  uint64_t nodeinfo_block = fs_get_block (sb);

	init_folder_struct (folder, folder_block, nodeinfo_block);
	int status = init_nodeinfo_struct (sb, dname, n_info, nodeinfo_block);

  if (status == invalid){
    free (father);
    father = NULL;

    free (folder);
    folder = NULL;

    free (n_info);
    n_info = NULL;

    errno = ENAMETOOLONG;
    return invalid;
  }

  /* Read father inode stored in file */
  SEEK_READ (sb, fileBlock, father);
  status = checkfather_path (sb, dname, father);

  if (status == invalid){
    free (father);
    father = NULL;

    free (folder);
    folder = NULL;

    free (n_info);
    n_info = NULL;
    
    errno = ENOENT;
    return invalid;
  }
 
  /* Ok, proceed */
  if (!exists){
		insertInBlockLinks (sb, fileBlock, folder_block);

		/* store both inodes related to the folder that
			 has been just created
		*/
    SEEK_WRITE (sb, folder_block, folder);
    SEEK_WRITE (sb, nodeinfo_block, n_info);
    
  }


  free (father);
  father = NULL;
  free (folder);
  folder = NULL;
  free (n_info);
  n_info = NULL;

	return success;
}
