#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>

#include "fs.h"
#include "utils.h"

int fs_delete_file(struct superblock *sb, const char *fname){	//o proprio nome ja diz.
	int i=0, found, maxLink;
	uint64_t fileBlock, fileLinkBlk, lastBlock, folderBlock;			//numero do bloco e do bloco do link no diretorio que tem ele

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

	if(found == 0){				//arquivo nao existe
		errno = ENOENT;
		return -1;
	}

	SEEK_READ(sb, fileBlock, file);					//pegando inode do arquivo
	if(file->mode == IMDIR){				//se for diretorio
		errno = EISDIR;
		return -1;
	}
	
	folderBlock = file->parent;
	SEEK_READ(sb, folderBlock, folder);				//diretorio onde esta o arquivo
	SEEK_READ(sb, folder->meta, folderInfo);

	//removendo o arquivo e blocos associados
	fs_put_block(sb,file->meta);						//liberando bloco nodeinfo
	while(i < maxLink && file->links[i] != 0){
		fs_put_block(sb, file->links[i]);
		file->links[i] = 0;									//marcando que nao tem mais bloco neste link.
		i++;
		if(i == maxLink && file->next != 0){
			SEEK_READ(sb,file->next,file);
			fs_put_block(sb,file->meta);			//apagando bloco anterior depois de ir pro próximo.
			i = 0;
		}
	}
	
	//removendo na pasta tambem;
	i = 0;	
	while((folder->links[i % maxLink] != fileBlock) && i < folderInfo->size){
		i++;
		if((i % maxLink == 0) && (folder->next != 0)){
			fileLinkBlk = folder->next;							//guardando bloco que pode conter link pro arquivo. Saindo do while sera ele.
			SEEK_READ(sb, folder->next, folder);
		}
	}
	
	lastBlock = getNodeLastLinkBlock(sb, folderBlock);
	if(lastBlock == fileLinkBlk){
		folder->links[i % maxLink] = folder->links[getLinksLen(folder) - 1];
		folder->links[getLinksLen(aux) - 1] = 0;
	}
	else{
		SEEK_READ(sb,lastBlock,aux);
		folder->links[i % maxLink] = aux->links[getLinksLen(aux) - 1];		//copiando ultimo link pra posicao q indicava bloco do arquivo removido
		aux->links[getLinksLen(aux) - 1] = 0;
		SEEK_WRITE(sb,lastBlock,aux);      //Devo escrever o bloco alterado de volta no disco, certo?
	}
	folderInfo->size--;
	
	SEEK_WRITE(sb,fileLinkBlk,folder);		//escrevendo o que contem o link pro arquivo (foi alterado ali em cima)
	SEEK_READ(sb,folderBlock,folder);					//voltando pro inode principal da pasta
	SEEK_WRITE(sb,folder->meta,folderInfo);				//pra escrever o folderinfo no bloco onde ele estava.

	free(file);
	free(folder);
	free(folderInfo);
	free(aux);
	
	return 0;
}


char * fs_list_dir(struct superblock *sb, const char *dname){	//lista tudo o que tem dentro de dname.
	int i=0, found,size, isDir;
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
	names = (char *) malloc (sizeof(char));

	*names = '\0';							//comecara 'nulo'
	dirBlock = findFile(sb, dname, &found);			//vasculhandodo o diretorio
	
	if(found == 0){				//o dretorio nao existe
		errno = ENOENT;
		return NULL;
	}

	SEEK_READ(sb, dirBlock, dir);					//pegando inode do diretorio
	if(dir->mode != IMDIR){				//se nao for diretorio
		errno = ENOTDIR;
		return NULL;
	}
	
	while(i < maxLink && dir->links[i] != 0){								//buscar em todos os links do bloco

		SEEK_READ(sb,dir->links[i],aux);			//objeto de 'links', sempre sera inode IMREG ou IMDIR
		SEEK_READ(sb,aux->meta,auxinfo);			//nodeinfo dele

		if(aux->mode == IMDIR)
			isDir = 1;
		else
			isDir = 0;

		size = strlen(auxinfo->name) + 1 + isDir;			//pegando tamanho do nome mais um espaco e mais uma barra (ou nao)
		names = (char *)realloc(names, (size + strlen(names))*sizeof(char));	
		strcat(names,auxinfo->name);
		if(isDir)
			strcat(names, "/");
		strcat(names," ");							//concatenando espaco de separacao de nomes
		
		i++;
		if(i == maxLink && (dir->next != 0)){
			i = 0;													//tem next? reseta contagem
			SEEK_READ(sb,dir->next,dir);		//indo pro proximo inode, se houver.
		}
	}
	
	free(dir);
	free(aux);
	free(auxinfo);
	printf("%s",names);
	return names;

}
