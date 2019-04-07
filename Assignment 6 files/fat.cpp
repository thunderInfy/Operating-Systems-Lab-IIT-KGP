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

	//constructor
	memory(unsigned int file_sys_size ,unsigned long long numBlocks, unsigned int block_size){

		unsigned long long numBytesBitVec;					//number of bytes reserved for bit vector
		getNumBytesBitVec(numBlocks, numBytesBitVec);

		if(BIT_VECTOR_OFFSET + numBytesBitVec + DIRECTORY_INFO_MEMORY_ALLOCATION >= block_size){
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

	void getNumBytesBitVec(unsigned long long numBlocks, unsigned long long &numBytesBitVec){
		numBytesBitVec = numBlocks/8;
		
		if(numBlocks%8!=0)
			(numBytesBitVec)++;
	}

	void clearBlockContents(unsigned long long blockNum, unsigned int block_size){
		bzero((void*)(space + blockNum*(block_size)), block_size);
	}

	void writeInfo(unsigned long long blockNum, unsigned int block_size, unsigned int offset, const void *src, size_t n){
		memcpy((void*)(space + blockNum*block_size + offset),src,n);
	}

	void readInfo(unsigned long long blockNum, unsigned int block_size, unsigned int offset, void *dest, size_t n){
		memcpy(dest,(const void *)(space + blockNum*block_size + offset),n);
	}


	void updateBitVector_occupy(unsigned long long blockNum){
		this->space[BIT_VECTOR_OFFSET + blockNum/8] |= (1<<(7-blockNum%8));
	}

	void updateBitVector_free(unsigned long long blockNum){
		this->space[BIT_VECTOR_OFFSET + blockNum/8] &= ~(1<<(7-blockNum%8));
	}

	//function to create the super block
	void createSuperBlock(unsigned int file_sys_size, unsigned long long numBlocks, unsigned int block_size){

		unsigned long long numBytesBitVec;
		getNumBytesBitVec(numBlocks, numBytesBitVec);

		//clear block 0 where super block will be stored
		this->clearBlockContents(0, block_size);

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
		unsigned long long blockNumber = numBlocks - 1;

		strcpy(volName, "root");
		strcpy(dirName, "home");

		//storing block size at block size offset
		this->writeInfo(0, block_size, BLOCK_SIZE_OFFSET, (const void *)&(block_size), sizeof(block_size));
		
		//storing file system size at the respective offset
		this->writeInfo(0, block_size, FILE_SYS_SIZE_OFFSET, (const void *)&(file_sys_size), sizeof(file_sys_size));

		//storing volume name at the respective offset
		this->writeInfo(0, block_size, VOLUME_NAME_OFFSET, (const void *)&(volName), 8);
		
		//storing directory name at the respective offset
		this->writeInfo(0, block_size, BIT_VECTOR_OFFSET + numBytesBitVec, (const void *)&(dirName),DIRECTORY_INFO_MEMORY_ALLOCATION/2);

		//storing blockNumber of the home directory at the respective offset
		this->writeInfo(0, block_size, BIT_VECTOR_OFFSET + numBytesBitVec + DIRECTORY_INFO_MEMORY_ALLOCATION/2, (const void *)&(blockNumber),DIRECTORY_INFO_MEMORY_ALLOCATION/2);

	}

	//function to initialize FAT in block 1
	void initializeFAT(unsigned int block_size){

		//occupy block 1
		updateBitVector_occupy(1);

		// The data blocks of a file are maintained using a system-wide File Allocation Table (FAT)
		// It will be stored in Block-1.

		long long value = -1;
		unsigned int Offset = 0;

		//put value -1 for all FAT values
		for(;Offset<block_size; Offset+=sizeof(long long))
			this->writeInfo(1, block_size, Offset, (const void*)&value, sizeof(long long));

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
	disk->createSuperBlock(file_sys_size, numBlocks, block_size);

	//intialize FAT
	disk->initializeFAT(block_size);

}

// opens a file for reading/writing (create if not existing)
void my_open(){

}

// closes an already opened file
void my_close(){

}

// reads data from an already open file
void my_read(){

}

// writes data into an already open file
void my_write(){

}

// creates a new directory
void my_mkdir(){

}

// changes the working directory
void my_chdir(){

}

// removes a directory along with all its contents
void my_rmdir(){

}

// copies a file between Linux file system and your file system
void my_copy(){
	
}

// displays the contents of the specified file
void my_cat(){
	
}

int main(){
	return 0;
}
