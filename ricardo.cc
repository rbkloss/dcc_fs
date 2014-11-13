#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>

#include <assert.h>

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
    struct superblock* sb = (struct superblock*) malloc(sizeof (struct superblock));
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
        return 0;
    }
    uint64_t freeList = sb->freelist;
    struct freepage *fp = (struct freepage *) malloc(sizeof (freepage));
    lseek(sb->fd, freeList, SEEK_SET);
    read(sb->fd, fp, sb->blksz);
    if (fp->count != 0) {
        //update previous pointers
        struct freepage *fp_prev = (struct freepage *) malloc(sizeof (freepage));
        SEEK_READ(sb, fp->links[0], fp_prev);
        fp_prev->next = fp->next;
        SEEK_WRITE(sb, fp->links[0], fp_prev);
        free(fp_prev);
    }

    if (fp->next != 0) {
        //update next pointers
        struct freepage *fp_next = (struct freepage *) malloc(sizeof (freepage));
        SEEK_READ(sb, fp->next, fp_next);
        fp_next->links[0] = fp->links[0];
        SEEK_WRITE(sb, fp->next, fp_next);
        free(fp_next);
    }

    sb->freelist = fp->next;
    free(fp);

    return sb->freelist;
}

int fs_put_block(struct superblock *sb, uint64_t block) {
    if (sb->freeblks != 0) {
        uint64_t freeList = sb->freelist;
        struct freepage *fp = NULL, *fp_next = NULL;
        fp = (struct freepage *) malloc(sizeof (freepage));
        fp_next = (struct freepage *) malloc(sizeof (freepage));

        SEEK_READ(sb, freeList, fp_next);
        fp->next = freeList;
        fp->count = 0;
        SEEK_WRITE(sb, block, fp);
        fp_next->count = 1;
        fp_next->links[0] = block;
        SEEK_WRITE(sb, freeList, fp_next);

        sb->freelist = block;
        free(fp);
        free(fp_next);
    } else {
        struct freepage *fp = NULL;
        fp = (struct freepage *) malloc(sizeof (freepage));
        fp->next = 0;
        fp->count = 0;
        SEEK_WRITE(sb, block, fp);
        sb->freelist = block;
        sb->freeblks++;
        free(fp);
    }

    return 0;
}

int fs_write_file(struct superblock *sb, const char *fname, char *buf, size_t cnt) {
    const int blocksNeeded = cnt / sb->blksz;
    if (blocksNeeded > sb->freeblks) {
        errno = ENOSPC;
        return -1;
    }
    const int linkMaxLen = getINodeLinksCap(sb);
    const int linkBlocksNeeded = getINodeLinksCap(sb) / blocksNeeded;
    int blocksUsed = 0;

    struct inode* parentNode = NULL, *fileNode = NULL;
    struct nodeinfo* meta = NULL;
    uint64_t parentBlock = findFile(sb, fname);
    uint64_t prev = parentBlock;
    SEEK_READ(sb, parentBlock, parentNode);
    SEEK_READ(sb, parentNode->meta, meta);

    if (strcmp(meta->name, fname) == 0) {
        errno = EEXIST;
        return -1;
    } else {
        //fileBlock aponta pro nó mais próximo de onde o arquivo deve estar
        //isto é fileNode->parent = parentNode
        ////////////////////////////////////////

        const uint64_t fileRootBlock = fs_get_block(sb);
        uint64_t currBlock = fileRootBlock;

        int nLinks = getLinksLen(parentNode);
        int maxNumberOfFiles = getINodeLinksCap(sb) - nLinks;
        if (meta->size + 1 >= maxNumberOfFiles) {
            if(parentNode->)
            //TODO
            uint64_t dirSonBlock = fs_get_block(sb);
            
            parentNode->next = dirSonBlock;
            struct inode* dirSon = (struct inode*) malloc(sb->blksz);
            dirSon->links[0] = 
            
        } else {
            meta->size++;
            SEEK_WRITE(sb, parentNode->meta, meta);
            parentNode->links[nLinks] = fileRootBlock;
            parentNode->links[nLinks + 1] = 0;
            SEEK_WRITE(sb, parentBlock, parentNode);
        }
        //////////////

        fileNode = (struct inode*) malloc(sb->blksz);
        fileNode->mode = IMREG;
        fileNode->parent = parentBlock;
        fileNode->next = 0;
        strcpy(meta->name, fname);
        meta->size = cnt;

        uint64_t dataBlock = 0;
        int i = 0;
        int fileLinkLen = 0;
        while (blocksUsed < blocksNeeded) {
            //handle current block
            int i = 0;
            int fileLinkLen = 0;
            const int blocksToWrite = MAX(blocksNeeded, linkMaxLen);
            for (i = 0; i < blocksToWrite; i++) {
                dataBlock = fs_get_block(sb);
                SEEK_WRITE(sb, dataBlock, buf + blocksNeeded);
                fileNode->links[fileLinkLen++] = dataBlock;
            }
            blocksUsed += i - 1;

            //handle new block for adding the rest of the links
            if (blocksUsed > blocksNeeded) {
                SEEK_WRITE(sb, currBlock, fileNode);
                break;
            } else {
                /*
                 * Get new block, 
                 * make fileNode->next point to the new Block
                 * make fileNode the new Block
                 * fileNode->parent points to parentBlock...
                 * 
                 */
                uint64_t newLinkBlock = fs_get_block(sb);
                fileNode->next = newLinkBlock;
                SEEK_WRITE(sb, currBlock, fileNode);
                fileNode->mode = IMCHILD | IMREG;
                fileNode->parent = fileRootBlock;
                fileNode->meta = fs_get_block(sb);
                meta->name = prev;
                fileNode->next = 0;
                fileNode->links[0] = 0;
                prev = currBlock;
                currBlock = newLinkBlock;
            }
        }
    }
    SEEK_WRITE(sb, parentBlock, parentNode);

    return 0;
}
