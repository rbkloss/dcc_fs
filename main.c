#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

#include <assert.h>

#include "fs.h"
#define MKDIR

void test(uint64_t fsize, uint64_t blksz);
void fs_check(const struct superblock *sb, uint64_t fsize, uint64_t blksz);
void fs_free_check(struct superblock **sb, uint64_t fsize, uint64_t blksz);

void fs_io_test(uint64_t fsize, uint64_t blksz);

#define NELEMS(x) (sizeof(x)/sizeof(x[0]))

static char *fname = "img";

int main(int argc, char **argv) {
    uint64_t fsizes[] = {1 << 19, 1 << 20, 1 << 21, 1 << 23, 1 << 25};
    uint64_t blkszs[] = {64, 128, 256, 512, 1024};
    int i;

    for (i = 0; i < NELEMS(blkszs); i++) {
        printf("fsize %d blksz %d\n", (int) fsizes[i], (int) blkszs[i]);
        //test(fsizes[i], blkszs[i]);
    }
    for (i = 1; i < NELEMS(blkszs); i++) {
        printf("fsize %d blksz %d\n", (int) fsizes[i], (int) blkszs[i]);
        fs_io_test(fsizes[i], blkszs[i]);
    }



    exit(EXIT_SUCCESS);
}

void test(uint64_t fsize, uint64_t blksz) {
    int err;

    char *buf = malloc(fsize);
    if (!buf) {
        perror(NULL);
        exit(EXIT_FAILURE);
    }
    memset(buf, 0, fsize);

    unlink(fname);
    FILE *fd = fopen(fname, "w");
    fwrite(buf, 1, fsize, fd);
    fclose(fd);

    struct superblock *sb = fs_open(fname);
    if (errno != EBADF) {
        printf("FAIL did not set errno\n");
    }
    if (sb != NULL) {
        printf("FAIL unformatted img\n");
    }

    sb = fs_format(fname, blksz);
    err = errno;
    if (blksz < MIN_BLOCK_SIZE) {
        if (err != EINVAL) printf("FAIL did not set errno\n");
        if (sb != NULL) printf("FAIL formatted too small blocks\n");
    }
    if (fsize / blksz < MIN_BLOCK_COUNT) {
        if (err != ENOSPC) printf("FAIL did not set errno\n");
        if (sb != NULL) printf("FAIL formatted too small volume\n");
    }

    if (sb == NULL) return;

    fs_check(sb, fsize, blksz);
    fs_free_check(&sb, fsize, blksz);
    fs_check(sb, fsize, blksz);

    if (fs_close(sb)) perror("format_close");

    sb = fs_open(fname);
    if (!sb) perror("open");

    fs_check(sb, fsize, blksz);
    fs_free_check(&sb, fsize, blksz);
    fs_check(sb, fsize, blksz);

    if (fs_open(fname)) {
        printf("FAIL opened FS twice\n");
    } else if (errno != EBUSY) {
        printf("FAIL did not set errno EBUSY on fs reopen\n");
    }

    if (fs_close(sb)) perror("open_close");
}

void fs_io_test(uint64_t fsize, uint64_t blksz) {
    char *buf = malloc(fsize);
    if (!buf) {
        perror(NULL);
        exit(EXIT_FAILURE);
    }
    memset(buf, 0, fsize);
    char* imName = "file.img";
    unlink(imName);
    FILE *fd = fopen(imName, "w");
    fwrite(buf, 1, fsize, fd);
    fclose(fd);

    struct superblock*sb = fs_format(imName, blksz);
    if (sb == NULL) {
        free(buf);
        return;
    }

    char* buf_str = malloc(15);
    char* buf_str2 = malloc(6);
    char* buf_read = malloc(15);
    char* buf_read2 = malloc(6);
    char* fname = malloc(7);
    char* f2name = malloc(3);
    strcpy(buf_str2, "hallo");
    strcpy(fname, "/teste");
    strcpy(buf_str, "diga oi lilica");
#ifdef MKDIR
    strcpy(f2name, "/dir/a");
    if (fs_mkdir(sb, "/dir/") == -1) {
        perror("mkdir");
    }
#else
    strcpy(f2name, "/a");
#endif   

    if (fs_write_file(sb, fname, buf_str, strlen(buf_str) + 1) == -1) {
        perror("WriteFile Error!");
    }
    if (fs_read_file(sb, fname, buf_read, strlen(buf_str) + 1) == -1) {
        perror("ReadFile Error!");
    }

    if (fs_write_file(sb, f2name, buf_str2, strlen(buf_str2) + 1) == -1) {
        perror("WriteFile Error!");
    }
    if (fs_read_file(sb, f2name, buf_read2, strlen(buf_str2) + 1) == -1) {
        perror("ReadFile Error!");
    }
    printf("read string(%s), original(%s)\n", buf_read2, buf_str2);

    assert(strcmp(buf_str, buf_read) == 0);

    free(fs_list_dir(sb, "/"));
#ifdef MKDIR
    free(fs_list_dir(sb, "/dir"));
#endif
    if (fs_delete_file(sb, fname) == -1) {
        perror("Delete File: ");
    }
    free(fs_list_dir(sb, "/"));

    if (fs_close(sb)) perror("open_close");

    free(buf_read);
    free(buf_read2);
    free(fname);
    free(f2name);
    free(buf_str);
    free(buf_str2);
    free(buf);



    buf = NULL;
}

void fs_free_check(struct superblock **sb, uint64_t fsize, uint64_t blksz) {
    long long numblocks = fsize / blksz - (*sb)->freeblks;
    unsigned long long freeblks = (*sb)->freeblks;

    if (numblocks > 6) {
        printf("FAIL used more than 6 blocks on empty fs\n");
    }

    numblocks = fsize / blksz;

    char *blkmap = malloc(fsize / blksz);
    if (!blkmap) {
        perror(NULL);
        exit(EXIT_FAILURE);
    }
    memset(blkmap, 0, fsize / blksz);

    struct freepage *fp = malloc(blksz);
    if (!fp) {
        perror(NULL);
        exit(EXIT_FAILURE);
    }

    lseek((*sb)->fd, (*sb)->freelist * blksz, SEEK_SET);
    read((*sb)->fd, fp, blksz);

    uint64_t blknum = fs_get_block(*sb);
    while (blknum != 0 && blknum != ((uint64_t) - 1)) {
        unsigned long long llu = blknum;
        if (blknum >= numblocks) {
            printf("FAIL blknum %llu >= numblocks %llu\n", llu, numblocks);
        } else if (blkmap[blknum] != 0) {
            printf("FAIL blknum %llu returned before\n", llu);
        } else {
            blkmap[blknum] = 1;
        }
        blknum = fs_get_block(*sb);
    }
    if ((*sb)->freeblks != 0) printf("FAIL sb->freeblks != 0\n");

    fs_close(*sb);
    *sb = fs_open(fname);
    if ((*sb)->freeblks != 0) printf("FAIL reopen sb->freeblks != 0\n");

    int blocks_put = 0;
    for (uint64_t i = 0; i < numblocks; i++) {
        if (!blkmap[i]) continue;
        fs_put_block(*sb, i);
        blocks_put++;
    }
    if ((*sb)->freeblks != freeblks) printf("FAIL sb->freeblks != freeblks\n");
    //free(fp);
}

void fs_check(const struct superblock *sb, uint64_t fsize, uint64_t blksz) {
    if (sb->magic != 0xdcc605f5) {
        printf("FAIL magic\n");
    }
    if (sb->blks != fsize / blksz) {
        printf("FAIL number of blocks\n");
    }
    if (sb->blksz != blksz) {
        printf("FAIL block size\n");
    }

    struct inode *inode = malloc(blksz);
    if (!inode) {
        perror(NULL);
        exit(EXIT_FAILURE);
    }

    lseek(sb->fd, sb->root * blksz, SEEK_SET);
    read(sb->fd, inode, blksz);

    if (inode->mode != IMDIR) {
        printf("FAIL root IMDIR\n");
    }
    if (inode->next != 0) {
        printf("FAIL root next\n");
    }

    struct nodeinfo *info = malloc(blksz);
    if (!info) {
        perror(NULL);
        exit(EXIT_FAILURE);
    }

    lseek(sb->fd, inode->meta * blksz, SEEK_SET);
    read(sb->fd, info, blksz);

    if (info->size != 0) {
        printf("FAIL root size\n");
    }
    if (info->name[0] != '/' || info->name[1] != '\0') {
        printf("FAIL root name\n");
    }

    free(info);
    free(inode);
}