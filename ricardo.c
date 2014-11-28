
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
    lseek(sb->fd, 0, SEEK_SET);
    write(sb->fd, sb, blocksize);
    assert(sb->magic == 0xdcc605f5);
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
            fp->count = 0;
            lseek(sb->fd, i * blocksize, SEEK_SET);
            write(sb->fd, fp, blocksize);

            lseek(sb->fd, i * blocksize, SEEK_SET);
            read(sb->fd, fp, blocksize);
            assert(fp->next == i + 1);
        } else if (i + 1 >= sb->blks) {
            //last free block
            fp->next = 0;
            fp->count = 1;
            fp->links[0] = i - 1; //link to previous free block(doubly linked list)
            lseek(sb->fd, i * blocksize, SEEK_SET);
            write(sb->fd, fp, blocksize);

            lseek(sb->fd, i * blocksize, SEEK_SET);
            read(sb->fd, fp, blocksize);
            assert(fp->next == 0);
        } else {
            fp->next = i + 1;
            fp->count = 1;
            fp->links[0] = i - 1; //link to previous free block(doubly linked list)
            lseek(sb->fd, i * blocksize, SEEK_SET);
            write(sb->fd, fp, blocksize);

            lseek(sb->fd, i * blocksize, SEEK_SET);
            read(sb->fd, fp, blocksize);
            assert(fp->next == i + 1);
        }
    }

    free(fp);
    free(inode);
    free(info);

    return sb;
}

struct superblock * fs_open(const char *fname) {
    int fd = open(fname, O_RDWR);
    if (flock(fd, LOCK_EX | LOCK_NB) == -1) {
        errno = EBUSY;
        return NULL;
    }
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
    int blocksz = sb->blksz;
    free(sb);
    sb = malloc(blocksz);
    lseek(fd, 0, SEEK_SET);
    read(fd, sb, blocksz);
    return sb;
}

int fs_close(struct superblock *sb) {
    SEEK_WRITE(sb, 0, sb);
    if (flock(sb->fd, LOCK_UN | LOCK_NB) != 0) {
        errno = EBADF;
        return -1;
    }
    close(sb->fd);
    free(sb);
    return 0;
}

uint64_t fs_get_block(struct superblock *sb) {
    if (sb->freeblks == 0) {
        //report Error
        return 0;
    }
    uint64_t freeList = sb->freelist;
    struct freepage *fp = (struct freepage *) malloc(sb->blksz);
    struct freepage *fp_prev = (struct freepage *) malloc(sb->blksz);
    struct freepage *fp_next = (struct freepage *) malloc(sb->blksz);
    SEEK_READ(sb, freeList, fp);
    if (fp->count != 0) {
        //update previous pointers        
        SEEK_READ(sb, fp->links[0], fp_prev);
        fp_prev->next = fp->next;
        SEEK_WRITE(sb, fp->links[0], fp_prev);
    }

    if (fp->next != 0) {
        //update next pointers        
        SEEK_READ(sb, fp->next, fp_next);
        fp_next->links[0] = fp->links[0];
        SEEK_WRITE(sb, fp->next, fp_next);
    }

    sb->freelist = fp->next;
    sb->freeblks--;
    SEEK_WRITE(sb, 0, sb);

    free(fp_prev);
    free(fp);
    free(fp_next);
    return freeList;
}

int fs_put_block(struct superblock *sb, uint64_t block) {
    if (sb->freeblks != 0) {
        uint64_t freeList = sb->freelist;
        struct freepage *fp = NULL, *fp_next = NULL;
        fp = (struct freepage *) malloc(sb->blksz);
        fp_next = (struct freepage *) malloc(sb->blksz);

        SEEK_READ(sb, freeList, fp_next);
        fp->next = freeList;
        fp->count = 0;
        SEEK_WRITE(sb, block, fp);
        fp_next->count = 1;
        fp_next->links[0] = block;
        SEEK_WRITE(sb, freeList, fp_next);

        free(fp);
        free(fp_next);
    } else {
        struct freepage *fp = NULL;
        fp = (struct freepage *) malloc(sb->blksz);
        fp->next = 0;
        fp->count = 0;
        SEEK_WRITE(sb, block, fp);
        free(fp);
    }
    sb->freelist = block;
    sb->freeblks++;
    SEEK_WRITE(sb, 0, sb);
    return 0;
}

int fs_write_file(struct superblock *sb, const char *fname, char *buf, size_t cnt) {
    const uint64_t blocksNeeded = MAX(1, cnt / sb->blksz);
    if (blocksNeeded > sb->freeblks) {
        errno = ENOSPC;
        return -1;
    }
    int blocksUsed = 0;

    if (existsFile(sb, fname)) {
        errno = EEXIST;
        return -1;
    }

    int len = 0, exists = 0;    
    char** fileParts = getFileParts(fname, &len);
    uint64_t dirBlock = findFile(sb, fname,  &exists);
    char* dirName = NULL;
    struct inode* dirNode, *node;
    struct nodeinfo* meta = malloc(sb->blksz);
    initNode(&dirNode, sb->blksz);
    initNode(&node, sb->blksz);
    SEEK_READ(sb, dirBlock, dirNode);
    SEEK_READ(sb, dirNode->meta, meta);
    dirName = meta->name;
    if (dirBlock != sb->root) {
        if (strcmp(fileParts[MAX(0, len - 2)], dirName) != 0) {
            errno = EBADF;
            return -1;
        }
    }


    uint64_t fileBlock = fs_get_block(sb);
    insertInBlockLinks(sb, dirBlock, fileBlock);
    uint64_t blocksList[blocksNeeded];
    ///properly write the file
    while (blocksUsed < blocksNeeded) {
        uint64_t block = fs_get_block(sb);
        blocksList[blocksUsed] = block;
        char* blockContent = buf + sb->blksz * (blocksUsed++);
        SEEK_WRITE(sb, block, blockContent);
    }
    strcpy(meta->name, fileParts[len - 1]);
    meta->size = cnt;
    meta->reserved[0] = 0;

    node->meta = fs_get_block(sb);
    node->mode = IMREG;
    node->parent = dirBlock;

    SEEK_WRITE(sb, node->meta, meta);

    uint64_t nodeBlock = fileBlock;
    blocksUsed = 0;
    while (blocksUsed < blocksNeeded) {
        int i = 0;
        int linksLen = getLinksMaxLen(sb);
        int N = MIN(linksLen, blocksNeeded);
        while (i < N) {
            node->links[i] = blocksList[i];
            i++;
            blocksUsed++;
        }
        if (blocksUsed < blocksNeeded) {
            node->next = fs_get_block(sb);
            SEEK_WRITE(sb, nodeBlock, node);
            nodeBlock = node->next;
            node->next = 0;
            node->parent = fileBlock;
            node->mode = IMCHILD | IMREG;
            node->meta = fs_get_block(sb);
            strcpy(meta->name, fileParts[len - 1]);
            meta->size = cnt;
            SEEK_WRITE(sb, node->meta, meta);
        }
    }
    SEEK_WRITE(sb, nodeBlock, node);

    free(meta);
    free(node);
    freeFileParts(&fileParts, len);
    free(dirNode);

    return 0;
}

ssize_t fs_read_file(struct superblock *sb, const char *fname, char *buf,
        size_t bufsz) {
    if (existsFile(sb, fname) == FALSE) {
        errno = ENOENT;
        return -1;
    }
    int len = 0, exists = 0;
    char** fileParts = getFileParts(fname, &len);

    uint64_t fileBlock = findFile(sb, fname, &exists);

    struct inode* node = malloc(sb->blksz);
    struct nodeinfo* meta = malloc(sb->blksz);

    SEEK_READ(sb, fileBlock, node);
    SEEK_READ(sb, node->meta, meta);
    assert(strcmp(meta->name, fileParts[len - 1]) == 0);

    size_t size = MIN(meta->size, bufsz);
    size = MAX(sb->blksz, size);
    size_t read_blocks = 0;

    char* buf_p = malloc(size);
    char* p = buf_p;
    do {
        int i = 0;
        while (node->links[i] != 0) {
            SEEK_READ(sb, node->links[i++], p);
            p = p + sb->blksz;
            read_blocks++;
        }
        if (node->next == 0)break;
        SEEK_READ(sb, node->next, node);
    } while (node->next != 0);
    strcpy(buf, buf_p);
    freeFileParts(&fileParts, len);
    free(meta);
    free(node);
    free(buf_p);
    return size;
}
