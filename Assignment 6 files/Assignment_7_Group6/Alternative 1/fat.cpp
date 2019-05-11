#include "fat.h"

int check_existence_dir(char*,  int,  int&);
void getNumBlocks( int,  int,  int&);
void getMetaInfoFromDisk( int&,  int&,  int&,  int&, int&);
void updateOpenFileTable(directoryEntry, int, int, int&, int);
int goToBlockLevel(int, int, int&);
void allocateBlocks(int&, int&, int, int);

struct memory{
	char* space;							//the entire memory that we would use		

	//constructor
	memory( int file_sys_size , int numBlocks,  int block_size);

	//destructor
	~memory(){
		free(space);
	}

	void getNumBytesBitVec( int numBlocks,  int &numBytesBitVec);
	void createSuperBlock( int file_sys_size,  int numBlocks,  int block_size);
	void initializeFAT( int block_size);
	void getHomeDirOffset( int numBytesBitVec,  int &homeDirOffset);
	void clearBlockContents( int blockNum,  int block_size);
	void writeInfo(int blockNum, int block_size,  int offset, const void *src, size_t n);
	void getFATVal( int index,  int &value);
	void setFATVal( int index,  int value);
	void readInfo( int blockNum,  int offset, void *dest, size_t n);
	void getBlockSize( int &block_size);
	int  getBit( int blockNum);
	void getFreeBlock( int numBlocks,  int &freeBlock);
	void updateBitVector_occupy( int blockNum);
	void updateBitVector_free( int blockNum);
	void initializeOpenFileTable( int numBlocks,  int block_size);
	void initializeDirectory( int directoryBlockNumber,  int block_size);

};

memory::memory( int file_sys_size , int numBlocks,  int block_size){

	int numBytesBitVec;					//number of bytes reserved for bit vector
	getNumBytesBitVec(numBlocks, numBytesBitVec);

	int homeDirOffset;
	getHomeDirOffset(numBytesBitVec, homeDirOffset);

	if(homeDirOffset + DIRECTORY_INFO_MEMORY_ALLOCATION >= block_size*1024){
		perror("Block size is incapable of holding super block contents (even one directory)!");
		exit(-1);
	}

	if(((int)((block_size*1024)/sizeof(int)))<numBlocks){
		printf("Block Size : %d\nNo. of Blocks : %d\n",block_size,numBlocks);
		perror("Block size too small, FAT cannot be initialized properly!");
		exit(-1);

	}

	// Simulate the disk as a set of contiguous blocks in memory
	space = (char*)malloc(numBlocks * block_size * 1024);
}

void memory::updateBitVector_occupy( int blockNum){
	this->space[BIT_VECTOR_OFFSET + blockNum/8] |= (1<<(7-blockNum%8));
}

void memory::updateBitVector_free( int blockNum){
	this->space[BIT_VECTOR_OFFSET + blockNum/8] &= ~(1<<(7-blockNum%8));
}

void memory::initializeOpenFileTable( int numBlocks,  int block_size){

	/*
	Single entry of open file table:
	FAT index (-1 means invalid) , File Open Count
	*/
	//occupying the last block for open file table
	updateBitVector_occupy(numBlocks - 1);

	openFileTableEntry initialStructure;

	for( int Offset = 0; Offset < (block_size*1024); Offset+=sizeof(initialStructure))
		this->writeInfo(numBlocks-1, block_size, Offset, (const void*)&initialStructure, sizeof(initialStructure));
}

void memory::initializeDirectory( int directoryBlockNumber,  int block_size){

	//occupy the block
	updateBitVector_occupy(directoryBlockNumber);

	directoryEntry dirEnt;

	for( int Offset = 0; Offset < (block_size*1024); Offset+=sizeof(dirEnt))
		this->writeInfo(directoryBlockNumber, block_size, Offset, (const void*)&dirEnt, sizeof(dirEnt));

}

void memory::readInfo( int blockNum,  int offset, void *dest, size_t n){
	int block_size;
	getBlockSize(block_size);

	if(offset >= (block_size*1024)){
		perror("Out of Block error!");
		exit(-1);
	}

	memcpy(dest,(const void *)(space + blockNum*block_size*1024 + offset),n);
}

void memory::getBlockSize( int &block_size){
	memcpy(&block_size,(const void *)(space + BLOCK_SIZE_OFFSET),sizeof(block_size));
}

int memory::getBit( int blockNum){
	return (this->space[BIT_VECTOR_OFFSET + blockNum/8] & (1<<(7-blockNum%8)));
}

void memory::getFreeBlock( int numBlocks,  int &freeBlock){
	
	for(freeBlock = 2; freeBlock<numBlocks ; freeBlock++){

		if(getBit(freeBlock) == 0){
			return;
		}

	}

	perror("All blocks filled!");
	exit(-1);
}


void memory::getFATVal( int index,  int &value){
	readInfo(1, index*sizeof( int), &value, sizeof( int));
}

void memory::setFATVal( int index,  int value){
	int block_size;
	getBlockSize(block_size);

	writeInfo(1, block_size, index*sizeof( int), (const void *)&value, sizeof( int));
}

void memory::writeInfo(int blockNum, int block_size,  int offset, const void *src, size_t n){
	memcpy((void*)(space + blockNum*block_size*1024 + offset),src,n);
}

void memory::getNumBytesBitVec( int numBlocks,  int &numBytesBitVec){
	
	numBytesBitVec = numBlocks/8;
	
	if(numBlocks%8!=0)
		(numBytesBitVec)++;

}

//function to create the super block
void memory::createSuperBlock( int file_sys_size,  int numBlocks,  int block_size){

	 int numBytesBitVec;
	getNumBytesBitVec(numBlocks, numBytesBitVec);

	 int homeDirOffset;
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
	int blockNumber = numBlocks - 2;

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
	this->writeInfo(0, block_size, homeDirOffset + 12, (const void *)&(blockNumber),sizeof(int));

}

//function to initialize FAT in block 1
void memory::initializeFAT( int block_size){

	//occupy block 1
	updateBitVector_occupy(1);

	// The data blocks of a file are maintained using a system-wide File Allocation Table (FAT)
	// It will be stored in Block-1.

	int value = -1;
	int Offset = 0;

	//put value -1 for all FAT values
	for(;Offset<(block_size*1024); Offset+=sizeof( int))
		this->writeInfo(1, block_size, Offset, (const void*)&value, sizeof(int));

}

void memory::getHomeDirOffset( int numBytesBitVec,  int &homeDirOffset){
	homeDirOffset = BIT_VECTOR_OFFSET + numBytesBitVec + 16;
}

void memory::clearBlockContents( int blockNum,  int block_size){
	bzero((void*)(space + blockNum*block_size*1024), block_size*1024);
}

void getNumBlocks( int file_sys_size,  int block_size,  int &numBlocks){
	numBlocks = file_sys_size/block_size;		//get number of blocks
}

memory *disk;


void getMetaInfoFromDisk( int &file_sys_size,  int &block_size,  int &numBlocks,  int &numBytesBitVec,  int &homeDirOffset){

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

void generateFileSystem(int file_sys_size,  int block_size){
	// These are the inputs during the file system creation
	// file_sys_size : Size of the file system in KB	(Typical Value : 65536	KB)
	// block_size	 : Block Size in KB 				(Typical Value : 1		KB)

	int numBlocks;									//variable for number of blocks
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

void updateOpenFileTable(directoryEntry dirEnt,  int numBlocks,  int block_size,  int &Offset,  int type){

	openFileTableEntry openFileEnt;
	openFileEnt.FATIndex = dirEnt.FATIndex;
	openFileEnt.type = type;

	openFileTableEntry temp;
	temp.FATIndex = 0;

	for(Offset = 0; temp.FATIndex != -1; Offset+=sizeof(temp))
		disk->readInfo(numBlocks-1, Offset, &temp, sizeof(temp));

	Offset-=sizeof(temp);

	disk->writeInfo(numBlocks-1, block_size, Offset, (const void*)&openFileEnt, sizeof(openFileEnt));

}


// opens a file for reading/writing (create if not existing)
int my_open(char* filename,  int type){

	if(type!=READ_ONLY && type!=WRITE_ONLY && type!=READ_WRITE){
		perror("The parameter type should be either READ_ONLY or WRITE_ONLY or READ_WRITE");
		return -1;
	}

	if(strlen(filename) > 11){
		perror("Maximum length of file name can be 11");
		return -1;
	}

	 int numBytesBitVec;
	 int numBlocks;
	 int file_sys_size;
	 int block_size;
	 int homeDirOffset;
	 int workingDirBlockNum;

	getMetaInfoFromDisk(file_sys_size, block_size, numBlocks, numBytesBitVec, homeDirOffset);

	//getting working directory block number
	disk->readInfo(0, BIT_VECTOR_OFFSET + numBytesBitVec + 12, &workingDirBlockNum, sizeof(workingDirBlockNum));

	if(workingDirBlockNum <=1 ){
		perror("Invalid working directory");
		return -1;
	}

	directoryEntry dirEnt;
	 int Offset = 0;

	do{

		disk->readInfo(workingDirBlockNum, Offset, &dirEnt, sizeof(dirEnt));	
		
		if(strcmp(dirEnt.fileName, filename) == 0){
			//file found

			 int openFileTableOffset;

			updateOpenFileTable(dirEnt, numBlocks, block_size, openFileTableOffset, type);

			return openFileTableOffset;

		}

		Offset+=sizeof(dirEnt);	

	}while(dirEnt.FATIndex!=-1);

	Offset-=sizeof(dirEnt);

	//file not found, create
	 int freeBlock;

	//get free block
	disk->getFreeBlock(numBlocks, freeBlock);

	//update dirEnt details
	strcpy(dirEnt.fileName, filename);

	dirEnt.FATIndex = freeBlock;
	disk->writeInfo(workingDirBlockNum, block_size, Offset, &dirEnt, sizeof(dirEnt));

	//occupy and clear the block
	disk->updateBitVector_occupy(freeBlock);
	disk->clearBlockContents(freeBlock, block_size);

	int openFileTableOffset;

	updateOpenFileTable(dirEnt, numBlocks, block_size, openFileTableOffset, type);

	return openFileTableOffset;
}

// closes an already opened file
int my_close( int fd){

	 int numBlocks;
	 int file_sys_size;
	 int block_size;

	//reading file_sys_size from disk
	disk->readInfo(0, FILE_SYS_SIZE_OFFSET, &file_sys_size, sizeof(file_sys_size));

	//reading block_size from disk
	disk->getBlockSize(block_size);

	getNumBlocks(file_sys_size, block_size, numBlocks);

	if(fd >= block_size*1024){
		perror("Invalid file descriptor!");
		return -1; 
	}

	openFileTableEntry temp;
	disk->writeInfo(numBlocks-1, block_size, fd, (const void*)&temp, sizeof(temp));

	return 0;
}

int goToBlockLevel(int currentBlock, int blockLevel, int &blockLevelNumber){

	int curr = currentBlock;
	int levels = blockLevel;
	int value;

	while(levels--){

		disk->getFATVal(curr, value);

		if(value == -1){
			return -1;
		}

		curr = value;

	}

	blockLevelNumber = curr;

	return 0;

}


// reads data from an already open file
int my_read(int fd, char *buf, size_t count){

	bzero(buf, count);

	int numBytesBitVec;
	int numBlocks;
	int file_sys_size;
	int block_size;
	int homeDirOffset;

	getMetaInfoFromDisk(file_sys_size, block_size, numBlocks, numBytesBitVec, homeDirOffset);

	if(fd >= block_size*1024){
		perror("Invalid file descriptor!");
		return -1; 
	}

	openFileTableEntry temp;
	disk->readInfo(numBlocks-1, fd, &temp, sizeof(temp));

	if(temp.type!=READ_ONLY && temp.type!=READ_WRITE){
		perror("No access for read operation!");
		return -1;
	}

	//access for read is allowed
	int bytesRead = 0;
	int blockLevel, blockOffset, blockLevelNumber;

	blockLevel = (temp.readFilePointerOffset)/(block_size*1024);
	blockOffset = (temp.readFilePointerOffset)%(block_size*1024);

	if(goToBlockLevel(temp.FATIndex, blockLevel, blockLevelNumber) < 0){
		perror("Read File Pointer already at end!");
		return 0;
	}

	int remainingBytesInBlockLevel = block_size*1024 - blockOffset;

	if((int)count <= remainingBytesInBlockLevel){
		disk->readInfo(blockLevelNumber, blockOffset, (void *)buf, count);
		temp.readFilePointerOffset+=count;
		disk->writeInfo(numBlocks-1, block_size, fd, &temp, sizeof(temp));
		return count;
	}
	else
		disk->readInfo(blockLevelNumber, blockOffset, (void *)buf, remainingBytesInBlockLevel);

	bytesRead = remainingBytesInBlockLevel;

	int remainingCount = count - bytesRead;
	int block_count = remainingCount/(block_size*1024);

	if(remainingCount%(block_size*1024) != 0)	block_count++;

	//proceeding towards reading more blocks for the current file
	int prev = blockLevelNumber, value, q;

	for(q=0;q<block_count;q++){

		disk->getFATVal(prev, value);

		if(value == -1){
			break;
		}

		prev = value;

		//Read from this block
		if(q==block_count-1){

			disk->readInfo(prev, 0, (void *)(buf+bytesRead), count - bytesRead);
			bytesRead = count;
		
		}
		else{

			disk->readInfo(prev, 0, (void *)(buf+bytesRead), block_size*1024);
			bytesRead += block_size*1024;
		
		}
	}

	temp.readFilePointerOffset+=bytesRead;
	disk->writeInfo(numBlocks-1, block_size, fd, &temp, sizeof(temp));

	return bytesRead;
}

void allocateBlocks(int &prev, int &value, int numBlocks, int block_size){ 

	int freeBlock;
	// prev = 5;
	// disk->getFATVal(prev, value);

	disk->getFATVal(prev, value);

	//printf("FAAAAATTTTTGET:%d,,,,%d\n",prev,value);

	if(value == -1){
		//q-th block is not assigned
		//we need to assign this block

		//get free block
		disk->getFreeBlock(numBlocks, freeBlock);

		//printf("FREE:%d\n",freeBlock);

		//occupy free block
		disk->updateBitVector_occupy(freeBlock);

		//clear free block
		disk->clearBlockContents(freeBlock, block_size);

		//update FAT
		disk->setFATVal(prev, freeBlock);

		//printf("FAAAAATTTTTSET:%d,,,,%d\n",prev,freeBlock);

		//update prev
		prev = freeBlock;
	}
	else{
		prev = value;
	}
}


// writes data into an already open file
int my_write(int fd, char *buf, size_t count){

	int numBytesBitVec;
	int numBlocks;
	int file_sys_size;
	int block_size;
	int homeDirOffset;

	getMetaInfoFromDisk(file_sys_size, block_size, numBlocks, numBytesBitVec, homeDirOffset);

	if(fd >= block_size*1024){
		perror("Invalid file descriptor!");
		return -1; 
	}

	openFileTableEntry temp;
	disk->readInfo(numBlocks-1, fd, &temp, sizeof(temp));

	if(temp.type!=WRITE_ONLY && temp.type!=READ_WRITE){
		perror("No access for write operation!");
		return -1;
	}

	//write operation is allowed
	int blockLevel, blockOffset;

	blockLevel = (temp.writeFilePointerOffset)/(block_size*1024);
	blockOffset = (temp.writeFilePointerOffset)%(block_size*1024);

	int q = 1, prev = temp.FATIndex, value;
	//printf("QQQ%d\n",temp.FATIndex );
	value = prev;

	for(; q<=blockLevel; q++){

		//these are intermediate blocks
		//check if the q-th block is assigned or not 
		//for this, we need the previous block number
		//use the previous block number to look into FAT
		allocateBlocks(prev,value,numBlocks,block_size);
	}

	int remainingBytesInBlockLevel = block_size*1024 - blockOffset;

	//write in blockLevel block, value contains the blockNumber corresponding to blockLevel
	
	if((int)count <= remainingBytesInBlockLevel){
		disk->writeInfo(value, block_size, blockOffset, (const void *)buf, count);
		temp.writeFilePointerOffset+=count;
		disk->writeInfo(numBlocks-1, block_size, fd, &temp, sizeof(temp));
		return count;
	}
	else
		disk->writeInfo(value, block_size, blockOffset, (const void *)buf, remainingBytesInBlockLevel);


	int remainingCount = count - remainingBytesInBlockLevel;
	int bytesWritten = remainingBytesInBlockLevel;
	int block_count = remainingCount/(block_size*1024);

	if(remainingCount%(block_size*1024) != 0)	block_count++;

	//proceeding towards allocating more blocks for the current file
	prev = value;

	for(q=0;q<block_count;q++){

		//printf("TTT%d\n",prev);
		allocateBlocks(prev,value,numBlocks,block_size);
		//printf("WWW%d\n",prev);
		//write on this block
		if(q==block_count-1){

			disk->writeInfo(prev, block_size, 0, (const void *)(buf+bytesWritten), count - bytesWritten);
			bytesWritten = count;
		
		}
		else{
			disk->writeInfo(prev, block_size, 0, (const void *)(buf+bytesWritten), block_size*1024);
			bytesWritten+=block_size*1024;
		}
	}


	temp.writeFilePointerOffset+=bytesWritten;
	disk->writeInfo(numBlocks-1, block_size, fd, &temp, sizeof(temp));
	return bytesWritten;
}

// creates a new directory
int my_mkdir(char* dir){

	if(strlen(dir)>11){
		perror("Maximum length of directory name can be 11");
		return -1;
	}

	 int numBytesBitVec;
	 int numBlocks;
	 int file_sys_size;
	 int block_size;
	 int homeDirOffset;

	getMetaInfoFromDisk(file_sys_size, block_size, numBlocks, numBytesBitVec, homeDirOffset);

	 int offset;

	if(check_existence_dir(dir, homeDirOffset, offset) >= 0 ){

		perror("Cannot create directory, it already exists!");
		return -1;

	}

	 int blockNum = numBlocks - 3;
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
	disk->writeInfo(0, block_size, offset + 12, (const void *)&(blockNum),sizeof( int));	

	return 0;

}

//returns the offset if the given directory exists
int check_existence_dir(char* dir,  int homeDirOffset,  int &offset){

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

	 int numBytesBitVec;
	 int numBlocks;
	 int file_sys_size;
	 int block_size;
	 int homeDirOffset;

	getMetaInfoFromDisk(file_sys_size, block_size, numBlocks, numBytesBitVec, homeDirOffset);

	 int offset;

	if(check_existence_dir(dir, homeDirOffset, offset)<0){
		perror("Directory not found!");
		return -1;
	}	

	//writing working directory details in the superblock
	disk->writeInfo(0, block_size, BIT_VECTOR_OFFSET + numBytesBitVec, (const void *)(dir), 12);

	 int workingDirBlockNum;

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