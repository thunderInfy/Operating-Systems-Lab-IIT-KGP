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

int check_existence_dir(char*, unsigned int, unsigned int&);

struct openFileTableEntry{

	int FATIndex;
	int filePointerOffset;

	openFileTableEntry(){
		this->FATIndex = -1;
		this->filePointerOffset = 0;
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

struct memory{
	char* space;							//the entire memory that we would use		

	//constructor
	memory(unsigned int file_sys_size ,unsigned int numBlocks, unsigned int block_size){

		unsigned int numBytesBitVec;					//number of bytes reserved for bit vector
		getNumBytesBitVec(numBlocks, numBytesBitVec);

		unsigned int homeDirOffset;
		getHomeDirOffset(numBytesBitVec, homeDirOffset);

		if(homeDirOffset + DIRECTORY_INFO_MEMORY_ALLOCATION >= block_size*1024){
			perror("Block size is incapable of holding super block contents (even one directory)!");
			exit(-1);
		}

		if(((block_size*1024)/sizeof(unsigned int))<numBlocks){
			printf("Block Size : %d\nNo. of Blocks : %d\n",block_size,numBlocks);
			perror("Block size too small, FAT cannot be initialized properly!");
			exit(-1);

		}

		// Simulate the disk as a set of contiguous blocks in memory
		space = (char*)malloc(numBlocks * block_size * 1024);
	}

	//destructor
	~memory(){
		free(space);
	}

	void getNumBytesBitVec(unsigned int numBlocks, unsigned int &numBytesBitVec){
		numBytesBitVec = numBlocks/8;
		
		if(numBlocks%8!=0)
			(numBytesBitVec)++;
	}

	void getHomeDirOffset(unsigned int numBytesBitVec, unsigned int &homeDirOffset){
		homeDirOffset = BIT_VECTOR_OFFSET + numBytesBitVec + 16;
	}

	void clearBlockContents(unsigned int blockNum, unsigned int block_size){
		bzero((void*)(space + blockNum*block_size*1024), block_size*1024);
	}

	void writeInfo(unsigned int blockNum, unsigned int block_size, unsigned int offset, const void *src, size_t n){
		memcpy((void*)(space + blockNum*block_size*1024 + offset),src,n);
	}

	void readInfo(unsigned int blockNum, unsigned int offset, void *dest, size_t n){
		unsigned int block_size;
		getBlockSize(block_size);

		if(offset >= (block_size*1024)){
			perror("Out of Block error!");
			exit(-1);
		}

		memcpy(dest,(const void *)(space + blockNum*block_size*1024 + offset),n);
	}

	void getBlockSize(unsigned int &block_size){
		memcpy(&block_size,(const void *)(space + BLOCK_SIZE_OFFSET),sizeof(block_size));
	}

	int getBit(unsigned int blockNum){
		return (this->space[BIT_VECTOR_OFFSET + blockNum/8] & (1<<(7-blockNum%8)));
	}

	void getFreeBlock(unsigned int numBlocks, unsigned int &freeBlock){
		
		for(freeBlock = 2; freeBlock<numBlocks ; freeBlock++){

			if(getBit(freeBlock) == 0){
				return;
			}

		}

		perror("All blocks filled!");
		exit(-1);
	}

	void updateBitVector_occupy(unsigned int blockNum){
		this->space[BIT_VECTOR_OFFSET + blockNum/8] |= (1<<(7-blockNum%8));
	}

	void updateBitVector_free(unsigned int blockNum){
		this->space[BIT_VECTOR_OFFSET + blockNum/8] &= ~(1<<(7-blockNum%8));
	}

	//function to create the super block
	void createSuperBlock(unsigned int file_sys_size, unsigned int numBlocks, unsigned int block_size){

		unsigned int numBytesBitVec;
		getNumBytesBitVec(numBlocks, numBytesBitVec);

		unsigned int homeDirOffset;
		getHomeDirOffset(numBytesBitVec, homeDirOffset);

		//clear block 0 where super block will be stored
		this->clearBlockContents(0, block_size);

		// Block 0 is the super block which contains:
		//		Name of variable 						|	Offset
		// 		Block size 								|	0
		// 		Size of the file system in MB 			|	8
		// 		Volume name								|	16
		// 		Bit vector 								|	24
		//		Working Directory 						|	BIT_VECTOR_OFFSET + numBytesBitVec
		// 		Pointer(s) to the directory(ies).		|	homeDirOffset

		// The free blocks are maintained as a bit vector stored in the super block. 
		// The number of bits will be equal to the number of blocks in the file system.
		// If the i-th bit is 0, then block i is free; else, it is occupied.
		updateBitVector_occupy(0);

		char volName[8];
		char dirName[12];

		bzero((void *)volName, 8);
		bzero((void *)dirName, 12);

		//block number where directory is stored, second last block
		unsigned int blockNumber = numBlocks - 2;

		initializeDirectory(numBlocks - 2, block_size);

		strcpy(volName, "root");
		strcpy(dirName, "home");

		//storing block size at block size offset
		this->writeInfo(0, block_size, BLOCK_SIZE_OFFSET, (const void *)&(block_size), sizeof(block_size));
		
		//storing file system size at the respective offset
		this->writeInfo(0, block_size, FILE_SYS_SIZE_OFFSET, (const void *)&(file_sys_size), sizeof(file_sys_size));

		//storing volume name at the respective offset
		this->writeInfo(0, block_size, VOLUME_NAME_OFFSET, (const void *)(volName), strlen(volName)+1);
		
		//working directory initially is nothing, not even home, so storing nothing

		//storing directory name at the respective offset
		this->writeInfo(0, block_size, homeDirOffset, (const void *)(dirName),strlen(dirName)+1);

		//storing blockNumber of the home directory at the respective offset
		this->writeInfo(0, block_size, homeDirOffset + 12, (const void *)&(blockNumber),sizeof(unsigned int));

	}

	//function to initialize FAT in block 1
	void initializeFAT(unsigned int block_size){

		//occupy block 1
		updateBitVector_occupy(1);

		// The data blocks of a file are maintained using a system-wide File Allocation Table (FAT)
		// It will be stored in Block-1.

		int value = -1;
		unsigned int Offset = 0;

		//put value -1 for all FAT values
		for(;Offset<(block_size*1024); Offset+=sizeof(unsigned int))
			this->writeInfo(1, block_size, Offset, (const void*)&value, sizeof(int));

	}

	void initializeOpenFileTable(unsigned int numBlocks, unsigned int block_size){

		/*
		Single entry of open file table:
		FAT index (-1 means invalid) , File Open Count
		*/
		//occupying the last block for open file table
		updateBitVector_occupy(numBlocks - 1);

		openFileTableEntry initialStructure;

		for(unsigned int Offset = 0; Offset < (block_size*1024); Offset+=sizeof(initialStructure))
			this->writeInfo(numBlocks-1, block_size, Offset, (const void*)&initialStructure, sizeof(initialStructure));
	}

	void initializeDirectory(unsigned int directoryBlockNumber, unsigned int block_size){

		//occupy the block
		updateBitVector_occupy(directoryBlockNumber);

		directoryEntry dirEnt;

		for(unsigned int Offset = 0; Offset < (block_size*1024); Offset+=sizeof(dirEnt))
			this->writeInfo(directoryBlockNumber, block_size, Offset, (const void*)&dirEnt, sizeof(dirEnt));

	}
};

void getNumBlocks(unsigned int file_sys_size, unsigned int block_size, unsigned int &numBlocks){
	numBlocks = (file_sys_size*1024)/block_size;		//get number of blocks
}

memory *disk;


void getMetaInfoFromDisk(unsigned int &file_sys_size, unsigned int &block_size, unsigned int &numBlocks, unsigned int &numBytesBitVec, unsigned int &homeDirOffset){

	//reading file_sys_size from disk
	disk->readInfo(0, FILE_SYS_SIZE_OFFSET, &file_sys_size, sizeof(file_sys_size));

	//reading block_size from disk
	disk->getBlockSize(block_size);

	getNumBlocks(file_sys_size, block_size, numBlocks);

	//get offset for home directory
	disk->getNumBytesBitVec(numBlocks, numBytesBitVec);
	disk->getHomeDirOffset(numBytesBitVec, homeDirOffset);
}

//block 0 : 0 to block_size*1024-1
//block 1 : block_size*1024 to 2*block_size*1024-1
//block 2 : 2*block_size*1024 to 3*block_size*1024 - 1

void generateFileSystem(unsigned int file_sys_size, unsigned int block_size){
	// These are the inputs during the file system creation
	// file_sys_size : Size of the file system in MB	(Typical Value : 64MB)
	// block_size	 : Block Size in KB 				(Typical Value : 1 KB)

	unsigned int numBlocks;									//variable for number of blocks
	getNumBlocks(file_sys_size, block_size, numBlocks);
	
	// Create memory dynamically
	disk = new memory(file_sys_size, numBlocks, block_size);

	//create super block
	disk->createSuperBlock(file_sys_size, numBlocks, block_size);

	//intialize FAT
	disk->initializeFAT(block_size);

	//initialize open file table
	disk->initializeOpenFileTable(numBlocks, block_size);
}

void updateOpenFileTable(directoryEntry dirEnt, unsigned int numBlocks, unsigned int block_size, unsigned int &Offset){

	openFileTableEntry openFileEnt;
	openFileEnt.FATIndex = dirEnt.FATIndex;

	openFileTableEntry temp;
	temp.FATIndex = 0;

	for(Offset = 0; temp.FATIndex != -1; Offset+=sizeof(temp))
		disk->readInfo(numBlocks-1, Offset, &temp, sizeof(temp));

	Offset-=sizeof(temp);

	disk->writeInfo(numBlocks-1, block_size, Offset, (const void*)&openFileEnt, sizeof(openFileEnt));

}


// opens a file for reading/writing (create if not existing)
int my_open(char* filename){

	if(strlen(filename) > 11){
		perror("Maximum length of file name can be 11");
		return -1;
	}

	unsigned int numBytesBitVec;
	unsigned int numBlocks;
	unsigned int file_sys_size;
	unsigned int block_size;
	unsigned int homeDirOffset;
	unsigned int workingDirBlockNum;

	getMetaInfoFromDisk(file_sys_size, block_size, numBlocks, numBytesBitVec, homeDirOffset);

	//getting working directory block number
	disk->readInfo(0, BIT_VECTOR_OFFSET + numBytesBitVec + 12, &workingDirBlockNum, sizeof(workingDirBlockNum));

	if(workingDirBlockNum <=1 ){
		perror("Invalid working directory");
		return -1;
	}

	directoryEntry dirEnt;
	unsigned int Offset = 0;

	do{

		disk->readInfo(workingDirBlockNum, Offset, &dirEnt, sizeof(dirEnt));	
		
		if(strcmp(dirEnt.fileName, filename) == 0){
			//file found

			unsigned int openFileTableOffset;

			updateOpenFileTable(dirEnt, numBlocks, block_size, openFileTableOffset);

			return openFileTableOffset;

		}

		Offset+=sizeof(dirEnt);	

	}while(dirEnt.FATIndex!=-1);

	Offset-=sizeof(dirEnt);

	//file not found, create
	unsigned int freeBlock;

	//get free block
	disk->getFreeBlock(numBlocks, freeBlock);

	//update dirEnt details
	strcpy(dirEnt.fileName, filename);

	dirEnt.FATIndex = freeBlock;
	disk->writeInfo(workingDirBlockNum, block_size, Offset, &dirEnt, sizeof(dirEnt));

	//occupy and clear the block
	disk->updateBitVector_occupy(freeBlock);
	disk->clearBlockContents(freeBlock, block_size);

	unsigned int openFileTableOffset;

	updateOpenFileTable(dirEnt, numBlocks, block_size, openFileTableOffset);

	return openFileTableOffset;;
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
int my_mkdir(char* dir){

	if(strlen(dir)>11){
		perror("Maximum length of directory name can be 11");
		return -1;
	}

	unsigned int numBytesBitVec;
	unsigned int numBlocks;
	unsigned int file_sys_size;
	unsigned int block_size;
	unsigned int homeDirOffset;

	getMetaInfoFromDisk(file_sys_size, block_size, numBlocks, numBytesBitVec, homeDirOffset);

	unsigned int offset;

	if(check_existence_dir(dir, homeDirOffset, offset) >= 0 ){

		perror("Cannot create directory, it already exists!");
		return -1;

	}

	unsigned int blockNum = numBlocks - 3;
	//find a free block
	while((disk->getBit(blockNum)) == 1)blockNum--;

	if(blockNum < 0){
		perror("All blocks filled!");
		return -1;
	}

	disk->initializeDirectory(blockNum, block_size);

	//free block found
	//storing directory name at the respective offset
	disk->writeInfo(0, block_size, offset, (const void *)(dir),strlen(dir)+1);

	//storing blockNumber of the home directory at the respective offset
	disk->writeInfo(0, block_size, offset + 12, (const void *)&(blockNum),sizeof(unsigned int));	

	return 0;

}

//returns the offset if the given directory exists
int check_existence_dir(char* dir, unsigned int homeDirOffset, unsigned int &offset){

	char dirname[12];
	offset = homeDirOffset;

	do{
	
		disk->readInfo(0, offset, dirname, 12);

		if(strcmp(dirname, dir)==0){
			return 0;
		}

		offset+=DIRECTORY_INFO_MEMORY_ALLOCATION;

	}while(dirname[0]!='\0');

	offset-=DIRECTORY_INFO_MEMORY_ALLOCATION;

	return -1;

}

// changes the working directory
int my_chdir(char* dir){

	unsigned int numBytesBitVec;
	unsigned int numBlocks;
	unsigned int file_sys_size;
	unsigned int block_size;
	unsigned int homeDirOffset;

	getMetaInfoFromDisk(file_sys_size, block_size, numBlocks, numBytesBitVec, homeDirOffset);

	unsigned int offset;

	if(check_existence_dir(dir, homeDirOffset, offset)<0){
		perror("Directory not found!");
		return -1;
	}	

	//writing working directory details in the superblock
	disk->writeInfo(0, block_size, BIT_VECTOR_OFFSET + numBytesBitVec, (const void *)(dir), 12);

	unsigned int workingDirBlockNum;

	//reading block of the working directory
	disk->readInfo(0, offset+12, &workingDirBlockNum, sizeof(workingDirBlockNum));

	disk->writeInfo(0, block_size, BIT_VECTOR_OFFSET + numBytesBitVec + 12, (const void *)&(workingDirBlockNum), sizeof(workingDirBlockNum));

	return 0;
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

	// generateFileSystem(32, 16);
	// my_chdir((char*)"home");
	// printf("%d\n",my_open((char*)"hey"));
	// printf("%d\n",my_open((char*)"hey"));
	// printf("%d\n",my_open((char*)"hey2"));

	return 0;
}
