#include <assert.h>
#include "utils.h"
#include "fs.h"
#include "StringProc.h"

void seek_write(const struct superblock* sb, const uint64_t to, void * n) {
    assert(sb != NULL && n != NULL);
    lseek((sb)->fd, (to) * (sb)->blksz, SEEK_SET);
    write((sb)->fd, (n), (sb)->blksz);
}

void seek_read(const struct superblock* sb, const uint64_t from, void* n) {
    assert(sb != NULL && n != NULL);
    lseek((sb)->fd, (from) * (sb)->blksz, SEEK_SET);
    read((sb)->fd, (n), sb->blksz);
}

void cleanNode(struct inode* n) {
    n->links[0] = 0;
    n->meta = 0;
    n->mode = 0;
    n->next = 0;
    n->parent = 0;
}

void initNode(struct inode** n, size_t sz) {
    *n = (struct inode*) malloc(sz);
    cleanNode(*n);
}

int getFileSize(const char* fname) {
    FILE *fd = fopen(fname, "r");
    fseek(fd, 0, SEEK_END);
    int size = ftell(fd);
    rewind(fd);
    fclose(fd);
    return size;
}

int getLinksMaxLen(const struct superblock* sb) {
    int ans = (sb->blksz - sizeof (struct inode)) / sizeof (uint64_t);
    return ans;
}

int getFileNameMaxLen(const struct superblock* sb) {
    int ans = sb->blksz - sizeof (struct nodeinfo);
    return ans - 1;
}

int getLinksLen(const struct inode* node) {
    int len = 0;
    while (node->links[len] != 0)len++;
    return len;
}

uint64_t getNodeLastLinkBlock(const struct superblock* sb, uint64_t nodeBlock) {
    uint64_t ans = nodeBlock;
    struct inode* node = NULL;
    node = malloc(sb->blksz);
    seek_read(sb, nodeBlock, node);
    while (node->next != 0) {
        ans = node->next;
        seek_read(sb, node->next, node);
    }
    free(node);
    return ans;
}

uint64_t findFile(const struct superblock* sb, const char* fname, int* exists) {
    assert(exists != NULL);
    struct inode* node, *ent;

    if (strcmp("/", fname) == 0) {
        *exists = TRUE;
        return sb->root;
    }

    initNode(&node, sb->blksz);
    initNode(&ent, sb->blksz);
    struct nodeinfo *meta = (struct nodeinfo*) malloc(sb->blksz);
    int len = 0;
    char** fileParts = getFileParts(fname, &len);

    *exists = FALSE;
    uint64_t fileBlock = sb->root;
    seek_read(sb, fileBlock, node);
    int it = 0;
    while (it < len) {
        int foundEnt = FALSE;
        int i = 0;
        do {
            while (node->links[i] != 0) {
                seek_read(sb, node->links[i], ent);
                seek_read(sb, ent->meta, meta);
                if (strcmp(meta->name, fileParts[it]) == 0) {
                    foundEnt = TRUE;
                    fileBlock = node->links[i];
                    it++;
                    break;
                }
                i++;
            }
            if ((foundEnt || node->next == 0))break;
            seek_read(sb, node->next, node);
        } while (node->next != 0);
        if (foundEnt) {
            seek_read(sb, fileBlock, node);
        } else {
            *exists = FALSE;
            free(node);
            free(ent);
            free(meta);
            freeFileParts(&fileParts, len);
            return fileBlock;
        }
    }

    if (it >= len) *exists = TRUE;
    freeFileParts(&fileParts, len);
    free(node);
    free(ent);
    free(meta);
    return fileBlock;
}

int existsFile(const struct superblock* sb, const char* fname) {
    int exists = 0;
    findFile(sb, fname, &exists);

    return (exists);
}

int InsertInNode(struct superblock* sb, const char* dirName,
        const uint64_t fileBlock) {
    int exists;
    struct inode*dirNode = NULL;
    struct nodeinfo* meta = NULL;
    dirNode = malloc(sb->blksz);
    meta = malloc(sb->blksz);
    uint64_t dirBlock = findFile(sb, dirName, &exists);
    seek_read(sb, dirBlock, dirNode);
    seek_read(sb, dirNode->meta, meta);
    int len = 0;
    char** fileparts = getFileParts(dirName, &len);
    if (strcmp(meta->name, fileparts[len - 1]) != 0)return 0; //dir does not exists
    if ((++meta->size) % getLinksMaxLen(sb) == 0) {
        //needs to create another link block      
        struct inode* linknode = NULL;
        linknode = malloc(sb->blksz);
        linknode->links[0] = fileBlock;
        linknode->links[1] = 0;
        uint64_t lastLinkBlock = getNodeLastLinkBlock(sb, dirBlock);
        struct inode* lastLinkNode = NULL;
        lastLinkNode = malloc(sb->blksz);
        seek_read(sb, lastLinkBlock, lastLinkNode);
        if (lastLinkNode->next != 0) {
            exit(EXIT_FAILURE);
        }
        lastLinkNode->next = fs_get_block(sb);
        seek_write(sb, lastLinkNode->next, linknode);
        seek_write(sb, lastLinkBlock, lastLinkNode);
        free(linknode);
        free(lastLinkNode);
    } else {
        int linkLen = getLinksLen(dirNode);
        dirNode->links[linkLen] = fileBlock;
        dirNode->links[linkLen + 1] = 0;
    }
    seek_write(sb, dirBlock, dirNode);
    seek_write(sb, dirNode->meta, meta);

    freeFileParts(&fileparts, len);
    free(dirNode);
    free(meta);
    return FALSE;
}

/**
 * Inserts block2Add in the links relative to destBlock.
 * Does not check if destBlock is valid!
 * @param sb the superblock
 * @param destBlock block to receive block2Add as a child
 * @param block2Add block to be added in the children of destBlock
 * @return as of now you can ignore this
 */
int insertInBlock(struct superblock* sb, const uint64_t destBlock,
        const uint64_t block2Add) {
    struct inode *dirNode = NULL;
    struct nodeinfo* meta = NULL;
    dirNode = malloc(sb->blksz);
    meta = malloc(sb->blksz);
    seek_read(sb, destBlock, dirNode);
    seek_read(sb, dirNode->meta, meta);
    if ((++meta->size) % getLinksMaxLen(sb) == 0) {
        //needs to create another link block      
        struct inode* linknode = NULL;
        linknode = malloc(sb->blksz);
        linknode->links[0] = block2Add;
        linknode->links[1] = 0;
        uint64_t lastLinkBlock = getNodeLastLinkBlock(sb, destBlock);
        struct inode* lastLinkNode = NULL;
        lastLinkNode = malloc(sb->blksz);
        seek_read(sb, lastLinkBlock, lastLinkNode);
        if (lastLinkNode->next != 0) {
            exit(EXIT_FAILURE);
        }
        lastLinkNode->next = fs_get_block(sb);
        seek_write(sb, lastLinkNode->next, linknode);
        seek_write(sb, lastLinkBlock, lastLinkNode);
        free(linknode);
        free(lastLinkNode);
    } else {
        int linkLen = getLinksLen(dirNode);
        dirNode->links[linkLen] = block2Add;
        dirNode->links[linkLen + 1] = 0;
    }
    seek_write(sb, destBlock, dirNode);
    seek_write(sb, dirNode->meta, meta);

    free(dirNode);
    free(meta);
    return TRUE;
}
