#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>

#include "fs.h"
#include "utils.h"

#define SB_SIZE 52

/* Build a new filesystem image in =fname (the file =fname should be present
 * in the OS's filesystem).  The new filesystem should use =blocksize as its
 * block size; the number of blocks in the filesystem will be automatically
 * computed from the file size.  The filesystem will be initialized with an
 * empty root directory.  This function returns NULL on error and sets errno
 * to the appropriate error code.  If the block size is smaller than
 * MIN_BLOCK_SIZE bytes, then the format fails and the function sets errno to
 * EINVAL.  If there is insufficient space to store MIN_BLOCK_COUNT blocks in
 * =fname, then the function fails and sets errno to ENOSPC. */
struct superblock * fs_format(const char *fname, uint64_t blocksize) {

    struct superblock *sb;
    struct inode *inode;
    struct nodeinfo *info;
    struct freepage *fp;
    int size;
    uint64_t i;

    if (blocksize < MIN_BLOCK_SIZE) {
        errno = EINVAL;
        return NULL;
    }

    sb = (struct superblock*) malloc(blocksize);
    inode = (struct inode*) malloc(blocksize);
    info = (struct nodeinfo*) malloc(blocksize);
    fp = (struct freepage*) malloc(blocksize);

    size = getFileSize(fname);


    //sb setup
    sb->fd = open(fname, O_RDWR);
    sb->magic = 0xdcc605f5;
    sb->root = 1;
    sb->blksz = blocksize;
    sb->blks = size / blocksize;
    sb->freeblks = sb->blks - 4;
    sb->freelist = 3;

    if (sb->blks < MIN_BLOCK_COUNT) {
        errno = ENOSPC;
        close(sb->fd);
        free(fp);
        free(inode);
        free(info);
        free(sb);
        return NULL;
    }

    //inode setup
    inode->parent = 1; //root points to itself
    inode->mode = IMDIR;
    inode->meta = 2; // metadata pointer
    inode->next = 0;
    // metadata setup
    info->size = 0; //there's no ent in this dir
    info->name[0] = '/'; // root name
    info->name[1] = '\0'; //string ending escape

    // file writeup
    lseek(sb->fd, 0, SEEK_SET);
    write(sb->fd, sb, blocksize);
    /////////////////////////////
    lseek(sb->fd, sb->root * blocksize, SEEK_SET);
    write(sb->fd, inode, blocksize);
    /////////////////////////////    
    lseek(sb->fd, inode->meta * blocksize, SEEK_SET);
    write(sb->fd, info, blocksize);


    //free page setup
    for (i = sb->freelist; i < sb->blks; i++) {
        if (i == sb->freelist) {
            //first freeBlock
            fp->next = i + 1;
        } else if (i + 1 >= sb->blks) {
            //last free block
            fp->next = 0;
            fp->count = 1;
            fp->links[0] = i - 1; //link to previous free block(doubly linked list)
        } else {
            fp->next = i + 1;
            fp->count = 1;
            fp->links[0] = i - 1; //link to previous free block(doubly linked list)
        }

        lseek(sb->fd, i * blocksize, SEEK_SET);
        write(sb->fd, fp, blocksize);
    }

    free(fp);
    free(inode);
    free(info);

    return sb;
}

struct superblock * fs_open(const char *fname) {
    int fd = open(fname, O_RDWR);
    struct superblock* sb = malloc(sizeof (struct superblock));
    read(fd, sb, sizeof (struct superblock));

    /*
     * If =fname does not contain a
     * 0xdcc605fs, then errno is set to EBADF.
     */
    if (sb->magic != 0xdcc605f5) {
        errno = EBADF;
        close(fd);
        free(sb);
        return NULL;
    }
    free(sb);
    return sb;
}

int fs_close(struct superblock *sb) {
    close(sb->fd);
    free(sb);
    return 1;
}

uint64_t fs_get_block(struct superblock *sb) {
    if (sb->freeblks == 0) {
        //report Error
        return NULL;
    }
    uint64_t freeList = sb->freelist;
    struct freepage *fp = malloc(sizeof (freepage));
    lseek(sb->fd, freeList, SEEK_SET);
    read(sb->fd, fp, sb->blksz);
    if (fp->count != 0) {
        //update previous pointers
        struct freepage *fp_prev = malloc(sizeof (freepage));
        lseek(sb->fd, fp->links[0], SEEK_SET);
        read(sb->fd, fp_prev, sb->blksz);
        fp_prev->next = fp->next;
        write(sb->fd, fp_prev, sb->blksz);
    }

    if (fp->next != 0) {
        //update next pointers
        struct freepage *fp_next = malloc(sizeof (freepage));
        lseek(sb->fd, fp->next, SEEK_SET);
        read(sb->fd, fp_next, sb->blksz);
        fp_next->links[0] = fp->links[0];
        write(sb->fd, fp_next, sb->blksz);
    }

    free(fp);

    return sb->freelist;
}