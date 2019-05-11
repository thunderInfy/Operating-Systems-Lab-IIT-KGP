#ifndef FAT_HEADER
#define FAT_HEADER

#include <cstdlib>		//for malloc, exit
#include <cstdio>		//for perror
#include <strings.h>	//for bzero
#include <string.h>		//for memcpy

#define BLOCK_SIZE_OFFSET 						0
#define FILE_SYS_SIZE_OFFSET					8
#define VOLUME_NAME_OFFSET						16
#define BIT_VECTOR_OFFSET						24
#define DIRECTORY_INFO_MEMORY_ALLOCATION		16

#define READ_ONLY		0
#define WRITE_ONLY		1
#define READ_WRITE		2
#define INVALID_TYPE	3


struct openFileTableEntry{

	int FATIndex;
	int readFilePointerOffset;
	int writeFilePointerOffset;
	int type;

	openFileTableEntry(){
		this->FATIndex = -1;
		this->readFilePointerOffset = 0;
		this->writeFilePointerOffset = 0;
		this->type = INVALID_TYPE;
	}

};

struct directoryEntry{

	char fileName[12];
	int FATIndex;

	directoryEntry(){
		bzero((void *)(this->fileName), 12);
		this->FATIndex = -1;
	}

};

void generateFileSystem(int, int);
int my_open(char*, int);
int my_close(int);
int my_read(int, char*, size_t);
int my_write(int, char*, size_t);
int my_mkdir(char*);
int my_chdir(char*);
void my_rmdir();
void my_copy();
void my_cat();

#endif