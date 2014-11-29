/* 
 * File:   utils.h
 * Author: rbk
 *
 * Created on November 8, 2014, 10:55 PM
 */

#ifndef UTILS_H
#define	UTILS_H

#ifdef	__cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <inttypes.h>
#include "fs.h"

#define FALSE 0
#define TRUE 1
#define FOR_EACH(i, n) for((i) = 0; (i) < (n); (i)++)
#define MAX(a, b) ((a) > (b))? a : b
#define MIN(a, b) ((a) < (b))? a : b

    void seek_write(const struct superblock* sb, const uint64_t to, void * n);

    void seek_read(const struct superblock* sb, const uint64_t from, void* n);

    void cleanNode(struct inode* n);
    void initNode(struct inode** n, size_t sz);

    int getFileSize(const char* fname);

    uint64_t findFile(const struct superblock* sb, const char* fname, int* exists);

    int getLinksMaxLen(const struct superblock* sb);
    int getFileNameMaxLen(const struct superblock* sb);

    uint64_t getNodeLastLinkBlock(const struct superblock* sb, uint64_t linkBlock);

    int getLinksLen(const struct inode* node);

    int insertBlock2NodeLinks(struct superblock* sb, const char* dirName,
            const uint64_t fileBlock);
    int insertInBlockLinks(struct superblock* sb, const uint64_t dirBlock,
            const uint64_t fileBlock);
    int insertInBlock(struct superblock* sb, const uint64_t destBlock,
            const uint64_t insertionBlock);

    int existsFile(const struct superblock* sb, const char* fname);



#ifdef	__cplusplus
}
#endif

#endif	/* UTILS_H */

