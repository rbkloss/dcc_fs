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

#define MAX(a, b) ((a) > (b))? a : b;
#define MIN(a, b) ((a) < (b))? a : b;

#define SEEK_WRITE(sb,TO, p) lseek((sb)->fd, (TO) * (sb)->blksz, SEEK_SET); \
        write((sb)->fd, (p), (sb)->blksz);
#define SEEK_READ(sb, FROM, p) lseek((sb)->fd, (FROM) * (sb)->blksz, SEEK_SET); \
        read((sb)->fd, (p), (sb)->blksz);

    int getFileSize(const char* fname);

    uint64_t findFile(const struct superblock* sb, const char* fname);

    int getINodeLinksCap(struct superblock* sb);
    uint64_t getLinksLen(struct inode* node);



#ifdef	__cplusplus
}
#endif

#endif	/* UTILS_H */

