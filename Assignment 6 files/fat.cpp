// Objective : Implement a memory-resident file system in a memory block

#include <cstdlib>		//for malloc, exit
#include <cstdio>		//for perror
#include <strings.h>	//for bzero
#include <string.h>		//for memcpy

#define BLOCK_SIZE_OFFSET 						0
#define FILE_SYS_SIZE_OFFSET					8
#define VOLUME_NAME_OFFSET						16
#define BIT_VECTOR_OFFSET						24
#define DIRECTORY_INFO_MEMORY_ALLOCATION		16

struct memory{
	char* space;							//the entire memory that we would use
	unsigned long long numBlocks;			//total number of blocks as we would get from the input
	unsigned int block_size;				//block size as we would get
	unsigned int file_sys_size;				//file system size as we would get
	unsigned long long numBytesBitVec;		//number of bytes reserved for bit vector

	//constructor
	memory(unsigned int file_sys_size ,unsigned long long numBlocks, unsigned int block_size){

		this->file_sys_size = file_sys_size;
		this->numBlocks = numBlocks;
		this->block_size = block_size;

		this->numBytesBitVec = numBlocks/8;
		if(numBlocks%8!=0)
			(this->numBytesBitVec)++;

		if(BIT_VECTOR_OFFSET + this->numBytesBitVec + DIRECTORY_INFO_MEMORY_ALLOCATION >= block_size){
			//8 is for directory pointer, atleast one directory will be there
			perror("Block size is incapable of holding super block contents!");
			exit(-1);
		}

		if((block_size/sizeof(long long))<numBlocks){

			perror("Block size too small, FAT cannot be initialized properly!");
			exit(-1);

		}

		// Simulate the disk as a set of contiguous blocks in memory
		space = (char*)malloc(numBlocks * block_size * 1024);
	}

	void clearBlockContents(unsigned long long blockNum){

		checkValidityBlock(blockNum);
		bzero((void*)(space + blockNum*(this->block_size)), this->block_size);

	}

	void writeInfo(unsigned long long blockNum, unsigned int offset, const void *src, size_t n){

		checkValidityBlock(blockNum);
		checkValidityOffset(offset);
		memcpy((void*)(space + blockNum*(this->block_size) + offset),src,n);

	}

	void checkValidityBlock(unsigned long long blockNum){

		if(blockNum >= (this->numBlocks)){
			perror("Invalid block index");
			exit(-1);
		}

	}

	void checkValidityOffset(unsigned int offset){

		if(offset >= (this->block_size)){
			perror("Invalid offset specified");
			exit(-1);
		}

	}

	void updateBitVector_occupy(unsigned long long blockNum){

		checkValidityBlock(blockNum);

		this->space[BIT_VECTOR_OFFSET + blockNum/8] |= (1<<(7-blockNum%8));

	}

	void updateBitVector_free(unsigned long long blockNum){

		checkValidityBlock(blockNum);

		this->space[BIT_VECTOR_OFFSET + blockNum/8] &= ~(1<<(7-blockNum%8));
	}

	//function to create the super block
	void createSuperBlock(){

		//clear block 0 where super block will be stored
		this->clearBlockContents(0);

		// Block 0 is the super block which contains:
		//		Name of variable 						|	Offset
		// 		Block size 								|	0
		// 		Size of the file system in MB 			|	8
		// 		Volume name								|	16
		// 		Bit vector 								|	24
		// 		Pointer(s) to the directory(ies).		|	BIT_VECTOR_OFFSET + numBytesBitVec

		// The free blocks are maintained as a bit vector stored in the super block. 
		// The number of bits will be equal to the number of blocks in the file system.
		// If the i-th bit is 0, then block i is free; else, it is occupied.
		updateBitVector_occupy(0);

		char volName[8];
		char dirName[8];

		//block number where directory is stored
		unsigned long long blockNumber = this->numBlocks - 1;

		strcpy(volName, "root");
		strcpy(dirName, "home");

		//storing block size at block size offset
		this->writeInfo(0, BLOCK_SIZE_OFFSET, (const void *)&(this->block_size), sizeof(this->block_size));
		
		//storing file system size at the respective offset
		this->writeInfo(0, FILE_SYS_SIZE_OFFSET, (const void *)&(this->file_sys_size), sizeof(this->file_sys_size));

		//storing volume name at the respective offset
		this->writeInfo(0, VOLUME_NAME_OFFSET, (const void *)&(volName), 8);
		
		//storing directory name at the respective offset
		this->writeInfo(0, BIT_VECTOR_OFFSET + numBytesBitVec, (const void *)&(dirName),DIRECTORY_INFO_MEMORY_ALLOCATION/2);

		//storing blockNumber of the home directory at the respective offset
		this->writeInfo(0, BIT_VECTOR_OFFSET + numBytesBitVec + DIRECTORY_INFO_MEMORY_ALLOCATION/2, (const void *)&(blockNumber),DIRECTORY_INFO_MEMORY_ALLOCATION/2);

	}

	//function to initialize FAT in block 1
	void initializeFAT(){

		// The data blocks of a file are maintained using a system-wide File Allocation Table (FAT)
		// It will be stored in Block-1.

		long long value = -1;
		int Offset = 0;

		//put value -1 for all FAT values
		for(;Offset<(this->block_size); Offset+=sizeof(long long))
			this->writeInfo(1, Offset, (const void*)&value, sizeof(long long));

	}

};

memory *disk;

//block 0 : 0 to block_size-1
//block 1 : block_size to 2*block_size-1
//block 2 : 2*block_size to 3*block_size - 1

void generateFAT(unsigned int file_sys_size, unsigned int block_size){
	// These are the inputs during the file system creation
	// file_sys_size : Size of the file system in MB	(Typical Value : 64MB)
	// block_size	 : Block Size in KB 				(Typical Value : 1 KB)

	unsigned long long numBlocks;						//variable for number of blocks

	numBlocks = (file_sys_size*1024)/block_size;		//get number of blocks
	
	// Create memory dynamically
	disk = new memory(file_sys_size, numBlocks, block_size);

	//create super block
	disk->createSuperBlock();

	//intialize FAT
	disk->initializeFAT();

}

void my_open(){

}

int main(){
	return 0;
}
