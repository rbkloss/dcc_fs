
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

    sb = (struct superblock*) calloc(1, blocksize);
    inode = (struct inode*) calloc(1, blocksize);
    info = (struct nodeinfo*) calloc(1, blocksize);
    fp = (struct freepage*) calloc(1, blocksize);

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
    sb = (struct superblock*) malloc(blocksz);
    lseek(fd, 0, SEEK_SET);
    read(fd, sb, blocksz);
    return sb;
}

int fs_close(struct superblock *sb) {
    if (sb == NULL) {
        return -1;
    }
    seek_write(sb, 0, sb);
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
    seek_read(sb, freeList, fp);
    if (fp->count != 0) {
        //update previous pointers        
        seek_read(sb, fp->links[0], fp_prev);
        fp_prev->next = fp->next;
        seek_write(sb, fp->links[0], fp_prev);
    }

    if (fp->next != 0) {
        //update next pointers        
        seek_read(sb, fp->next, fp_next);
        fp_next->links[0] = fp->links[0];
        seek_write(sb, fp->next, fp_next);
    }

    sb->freelist = fp->next;
    sb->freeblks--;
    seek_write(sb, 0, sb);

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

        seek_read(sb, freeList, fp_next);
        fp->next = freeList;
        fp->count = 0;
        seek_write(sb, block, fp);
        fp_next->count = 1;
        fp_next->links[0] = block;
        seek_write(sb, freeList, fp_next);

        free(fp);
        free(fp_next);
    } else {
        struct freepage *fp = NULL;
        fp = (struct freepage *) malloc(sb->blksz);
        fp->next = 0;
        fp->count = 0;
        seek_write(sb, block, fp);
        free(fp);
    }
    sb->freelist = block;
    sb->freeblks++;
    seek_write(sb, 0, sb);
    return 0;
}

int fs_write_file(struct superblock *sb, const char *fname, char *buf, size_t cnt) {
    const uint64_t blocksNeeded = MAX(1, cnt / sb->blksz);
    if (blocksNeeded > sb->freeblks) {
        errno = ENOSPC;
        return -1;
    }
    int blocksUsed = 0;

    if (strlen(fname) + 1 > getFileNameMaxLen(sb)) {
        errno = ENAMETOOLONG;
        return -1;
    }

    if (existsFile(sb, fname)) {
        errno = EEXIST;
        return -1;
    }

    int len = 0, exists = 0;
    char** fileParts = getFileParts(fname, &len);
    uint64_t dirBlock = findFile(sb, fname, &exists);
    char* dirName = NULL;
    struct inode* dirNode, *node;
    struct nodeinfo* meta = (struct nodeinfo*) calloc(1, sb->blksz);
    initNode(&dirNode, sb->blksz);
    initNode(&node, sb->blksz);
    seek_read(sb, dirBlock, dirNode);
    seek_read(sb, dirNode->meta, meta);
    dirName = calloc(1, sizeof (char)* (strlen(meta->name) + 1));
    strcpy(dirName, meta->name);
    if (dirBlock != sb->root) {
        if (strcmp(fileParts[MAX(0, len - 2)], dirName) != 0) {
            errno = EBADF;
            free(dirName);
            free(node);
            free(meta);
            free(dirNode);
            return -1;
        }
    }


    uint64_t fileBlock = fs_get_block(sb);
    insertInBlock(sb, dirBlock, fileBlock);
    uint64_t blocksList[blocksNeeded];
    ///properly write the file
    while (blocksUsed < blocksNeeded) {
        uint64_t block = fs_get_block(sb);
        blocksList[blocksUsed] = block;
        char* temp = calloc(1, sb->blksz);
        char* blockContent = buf + sb->blksz * (blocksUsed);
        strcpy(temp, blockContent);
        blocksUsed++;
        seek_write(sb, block, temp);
        free(temp);
    }
    strcpy(meta->name, fileParts[len - 1]);
    meta->size = cnt;
    meta->reserved[0] = 0;

    node->meta = fs_get_block(sb);
    node->mode = IMREG;
    node->parent = dirBlock;

    seek_write(sb, node->meta, meta);

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
            seek_write(sb, nodeBlock, node);
            nodeBlock = node->next;
            node->next = 0;
            node->parent = fileBlock;
            node->mode = IMCHILD | IMREG;
            node->meta = fs_get_block(sb);
            strcpy(meta->name, fileParts[len - 1]);
            meta->size = cnt;
            seek_write(sb, node->meta, meta);
        }
    }
    seek_write(sb, nodeBlock, node);

    free(meta);
    free(node);
    freeFileParts(&fileParts, len);
    free(dirNode);
    free(dirName);

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

    struct inode* node;
    initNode(&node, sb->blksz);
    struct nodeinfo* meta = (struct nodeinfo*) calloc(1, sb->blksz);

    seek_read(sb, fileBlock, node);
    seek_read(sb, node->meta, meta);
    assert(strcmp(meta->name, fileParts[len - 1]) == 0);

    size_t size = MIN(meta->size, bufsz);
    size = MAX(sb->blksz, size);
    size_t read_blocks = 0;

    char* buf_p = (char*) calloc(1, size);
    char* p = buf_p;
    do {
        int i = 0;
        while (node->links[i] != 0) {
            seek_read(sb, node->links[i++], p);
            p = p + sb->blksz;
            read_blocks++;
        }
        if (node->next == 0)break;
        seek_read(sb, node->next, node);
    } while (node->next != 0);
    strcpy(buf, buf_p);
    freeFileParts(&fileParts, len);
    free(meta);
    free(node);
    free(buf_p);
    return size;
}

int fs_delete_file(struct superblock *sb, const char *fname) { //o proprio nome ja diz.
    int i = 0, found, maxLink, ultimo;
    uint64_t fileBlock, fileLinkBlk, lastBlock, folderBlock; //numero do bloco e do bloco do link no diretorio que tem ele

    struct inode *file;
    struct inode *folder;
    struct inode *aux;
    struct nodeinfo *folderInfo;

    maxLink = getLinksMaxLen(sb);
    file = (struct inode *) malloc(sb->blksz);
    folder = (struct inode *) malloc(sb->blksz);
    aux = (struct inode *) malloc(sb->blksz);
    folderInfo = (struct nodeinfo *) malloc(sb->blksz);

    fileBlock = findFile(sb, fname, &found);

    if (found == 0) { //arquivo nao existe
        errno = ENOENT;
        return -1;
    }

    seek_read(sb, fileBlock, file); //pegando inode do arquivo
    if (file->mode == IMDIR) { //se for diretorio
        errno = EISDIR;
        return -1;
    }

    folderBlock = file->parent;
    fileLinkBlk = folderBlock;
    seek_read(sb, folderBlock, folder); //diretorio onde esta o arquivo
    seek_read(sb, folder->meta, folderInfo);


    //removendo o arquivo e blocos associados
    fs_put_block(sb, file->meta); //liberando bloco nodeinfo
    while ((i < maxLink) && (file->links[i] != 0)) {
        //if (i == maxLink)
        fs_put_block(sb, file->links[i]);
        file->links[i] = 0; //marcando que nao tem mais bloco neste link.
        i++;
        if (i == maxLink && file->next != 0) {
            seek_read(sb, file->next, file);
            fs_put_block(sb, file->meta); //apagando bloco anterior depois de ir pro próximo.
            i = 0;
        }
    }
    //removendo na pasta tambem;
    ultimo = (folder->links[getLinksLen(folder) - 1] == fileBlock);
    if (ultimo) {
        folder->links[getLinksLen(folder) - 1] = 0;
        //folderInfo->size--;
    } else {
        i = 0;
        while ((folder->links[i % maxLink] != fileBlock) && i < folderInfo->size) {
            i++;
            if ((i % maxLink == 0) && (folder->next != 0)) {
                fileLinkBlk = folder->next; //guardando bloco que pode conter link pro arquivo. Saindo do while sera ele.
                seek_read(sb, folder->next, folder);
            }
        }
        lastBlock = getNodeLastLinkBlock(sb, folderBlock);
        if (lastBlock == fileLinkBlk) {
            folder->links[i % maxLink] = folder->links[getLinksLen(folder) - 1];
            folder->links[getLinksLen(folder) - 1] = 0;
        } else {
            seek_read(sb, lastBlock, aux);
            folder->links[i % maxLink] = aux->links[getLinksLen(aux) - 1]; //copiando ultimo link pra posicao q indicava bloco do arquivo removido
            aux->links[getLinksLen(aux) - 1] = 0;
            seek_write(sb, lastBlock, aux); //Devo escrever o bloco alterado de volta no disco, certo?
        }

    }
    folderInfo->size--;

    seek_write(sb, fileLinkBlk, folder); //escrevendo o que contem o link pro arquivo (foi alterado ali em cima)
    seek_read(sb, folderBlock, folder); //voltando pro inode principal da pasta
    seek_write(sb, folder->meta, folderInfo); //pra escrever o folderinfo no bloco onde ele estava.

    free(file);
    free(folder);
    free(folderInfo);
    free(aux);

    return 0;
}

char * fs_list_dir(struct superblock *sb, const char *dname) { //lista tudo o que tem dentro de dname.
    int i = 0, found, size, isDir;
    uint64_t dirBlock;
    int maxLink;
    char *names;

    struct inode *dir;
    struct inode *aux;
    struct nodeinfo *auxinfo;

    maxLink = getLinksMaxLen(sb);
    dir = (struct inode *) malloc(sb->blksz);
    aux = (struct inode *) malloc(sb->blksz);
    auxinfo = (struct nodeinfo *) malloc(sb->blksz);
    names = (char *) malloc(sizeof (char));

    strcpy(names, ""); //comecara 'nulo'

    dirBlock = findFile(sb, dname, &found); //vasculhandodo o diretorio

    if (found == 0) { //o dretorio nao existe
        errno = ENOENT;
        return NULL;
    }

    seek_read(sb, dirBlock, dir); //pegando inode do diretorio
    if (dir->mode != IMDIR) { //se nao for diretorio
        errno = ENOTDIR;
        return NULL;
    }

    while (i < maxLink && dir->links[i] != 0) { //buscar em todos os links do bloco

        seek_read(sb, dir->links[i], aux); //objeto de 'links', sempre sera inode IMREG ou IMDIR
        seek_read(sb, aux->meta, auxinfo); //nodeinfo dele

        if (aux->mode == IMDIR)
            isDir = 1;
        else
            isDir = 0;

        size = strlen(auxinfo->name) + 1 + isDir; //pegando tamanho do nome mais um espaco e mais uma barra (ou nao)
        char* temp = (char *) malloc((size + strlen(names) + 1) * sizeof (char));
        strcpy(temp, names);
        strcat(temp, auxinfo->name);
        free(names);
        names = temp;
        if (isDir)
            strcat(names, "/");
        strcat(names, " "); //concatenando espaco de separacao de nomes

        i++;
        if (i == maxLink && (dir->next != 0)) {
            i = 0; //tem next? reseta contagem
            seek_read(sb, dir->next, dir); //indo pro proximo inode, se houver.
        }
    }

    free(dir);
    free(aux);
    free(auxinfo);
    printf("%s\n", names);
    return names;
}

//int fs_delete_file(struct superblock *sb, const char *fname) { //o proprio nome ja diz.
//    int i = 0, found, maxLink;
//    uint64_t fileBlock, fileLinkBlk, lastBlock, folderBlock; //numero do bloco e do bloco do link no diretorio que tem ele
//
//    struct inode *file;
//    struct inode *folder;
//    struct inode *aux;
//    struct nodeinfo *folderInfo;
//
//    maxLink = getLinksMaxLen(sb);
//    file = (struct inode *) malloc(sb->blksz);
//    folder = (struct inode *) malloc(sb->blksz);
//    aux = (struct inode *) malloc(sb->blksz);
//    folderInfo = (struct nodeinfo *) malloc(sb->blksz);
//
//    fileBlock = findFile(sb, fname, &found);
//
//    if (found == 0) { //arquivo nao existe
//        errno = ENOENT;
//        return -1;
//    }
//
//    SEEK_READ(sb, fileBlock, file); //pegando inode do arquivo
//    if (file->mode == IMDIR) { //se for diretorio
//        errno = EISDIR;
//        return -1;
//    }
//
//    folderBlock = file->parent;
//    SEEK_READ(sb, folderBlock, folder); //diretorio onde esta o arquivo
//    SEEK_READ(sb, folder->meta, folderInfo);
//
//    //removendo o arquivo e blocos associados
//    fs_put_block(sb, file->meta); //liberando bloco nodeinfo
//    while (i < maxLink && file->links[i] != 0) {
//        fs_put_block(sb, file->links[i]);
//        file->links[i] = 0; //marcando que nao tem mais bloco neste link.
//        i++;
//        if (i == maxLink && file->next != 0) {
//            SEEK_READ(sb, file->next, file);
//            fs_put_block(sb, file->meta); //apagando bloco anterior depois de ir pro próximo.
//            i = 0;
//        }
//    }
//
//    //removendo na pasta tambem;
//    i = 0;
//    while ((folder->links[i % maxLink] != fileBlock) && i < folderInfo->size) {
//        i++;
//        if ((i % maxLink == 0) && (folder->next != 0)) {
//            fileLinkBlk = folder->next; //guardando bloco que pode conter link pro arquivo. Saindo do while sera ele.
//            SEEK_READ(sb, folder->next, folder);
//        }
//    }
//
//    lastBlock = getNodeLastLinkBlock(sb, folderBlock);
//    if (lastBlock == fileLinkBlk) {
//        folder->links[i % maxLink] = folder->links[getLinksLen(folder) - 1];
//        folder->links[getLinksLen(aux) - 1] = 0;
//    } else {
//        SEEK_READ(sb, lastBlock, aux);
//        folder->links[i % maxLink] = aux->links[getLinksLen(aux) - 1]; //copiando ultimo link pra posicao q indicava bloco do arquivo removido
//        aux->links[getLinksLen(aux) - 1] = 0;
//        SEEK_WRITE(sb, lastBlock, aux); //Devo escrever o bloco alterado de volta no disco, certo?
//    }
//    folderInfo->size--;
//
//    SEEK_WRITE(sb, fileLinkBlk, folder); //escrevendo o que contem o link pro arquivo (foi alterado ali em cima)
//    SEEK_READ(sb, folderBlock, folder); //voltando pro inode principal da pasta
//    SEEK_WRITE(sb, folder->meta, folderInfo); //pra escrever o folderinfo no bloco onde ele estava.
//
//    free(file);
//    free(folder);
//    free(folderInfo);
//    free(aux);
//
//    return 0;
//}
//
//char * fs_list_dir(struct superblock *sb, const char *dname) { //lista tudo o que tem dentro de dname.
//    int i = 0, found, size, isDir;
//    uint64_t dirBlock;
//    int maxLink;
//    char *names;
//
//    struct inode *dir;
//    struct inode *aux;
//    struct nodeinfo *auxinfo;
//
//    maxLink = getLinksMaxLen(sb);
//    initNode(&dir, sb->blksz);
//    initNode(&aux, sb->blksz);
//    auxinfo = (struct nodeinfo *) malloc(sb->blksz);
//    names = (char *) malloc(sizeof (char) * 10);
//
//    strcpy(names, "");
//    dirBlock = findFile(sb, dname, &found); //vasculhandodo o diretorio
//
//    if (found == 0) { //o dretorio nao existe
//        errno = ENOENT;
//        return NULL;
//    }
//
//    SEEK_READ(sb, dirBlock, dir); //pegando inode do diretorio
//    if (dir->mode != IMDIR) { //se nao for diretorio
//        errno = ENOTDIR;
//        return NULL;
//    }
//
//    while (i < maxLink && dir->links[i] != 0) { //buscar em todos os links do bloco
//
//        SEEK_READ(sb, dir->links[i], aux); //objeto de 'links', sempre sera inode IMREG ou IMDIR
//        SEEK_READ(sb, aux->meta, auxinfo); //nodeinfo dele
//
//        if (aux->mode == IMDIR)
//            isDir = 1;
//        else
//            isDir = 0;
//
//        size = strlen(auxinfo->name) + 1 + isDir; //pegando tamanho do nome mais um espaco e mais uma barra (ou nao)
//        names = (char *) realloc(names, (size + strlen(names)) * sizeof (char));
//        strcat(names, auxinfo->name);
//        if (isDir)
//            strcat(names, "/");
//        strcat(names, " "); //concatenando espaco de separacao de nomes
//
//        i++;
//        if (i == maxLink && (dir->next != 0)) {
//            i = 0; //tem next? reseta contagem
//            SEEK_READ(sb, dir->next, dir); //indo pro proximo inode, se houver.
//        }
//    }
//
//    free(dir);
//    free(aux);
//    free(auxinfo);
//    printf("%s\n", names);
//    return names;
//
//}
//
