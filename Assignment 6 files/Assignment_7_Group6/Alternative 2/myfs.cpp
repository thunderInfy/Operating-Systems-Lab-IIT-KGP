#include "myfs.h"

struct superblock {
	char volume_name[8];
	unsigned long long block_size;
	unsigned long long total_size;
	int first_free_block;
};

struct inode {
	bool valid;										//valid means it has some file associated with it
	bool file;										//true if it represents a file
	unsigned long long file_size;					//stores size of file
	int dp[NUMBER_OF_DP];							//direct pointers
	int sip;										//singly indirect pointers
	int dip;										//doubly indirect pointers
};

struct datablock {
	char *data;					//data is pointer to the starting location of the block in the memory
	int *next;					//pointer to the next datablock index
	unsigned long long dsize;	//data size = block size - sizeof(int)

	datablock(char *ptr, unsigned long long bsize) {

		//ptr is pointer to the starting location of the block in the memory
		//bsize is block_size

		data = ptr;
		dsize = bsize - sizeof(int);
		next = (int *)(ptr + dsize);		//next is a dangling pointer unless set_pointer is called
	}

	~datablock() {
	}

	void set_pointer(int nextblock) {
		*next = nextblock;
	}

	int get_pointer() {
		return *next;
	}

	void eraseDataBlock(){
		bzero(data, dsize + sizeof(int));
	}

	unsigned long long set_data(char *val, unsigned long long len, unsigned long long offset = 0) {
		
		//val is the data buffer

		//if length to write is greater than the available space in the data block
		if(len > (dsize - offset))
			len = dsize - offset;
	
		memcpy(data + offset, val, len);
		return len;
	}

	unsigned long long get_data(char *val, unsigned long long len, unsigned long long offset = 0) {

		//val is the data buffer

		//if length to read is greater than the available space
		if(len > (dsize - offset))
			len = dsize - offset;

		memcpy(val, data + offset, len);
		return len;
	}
};

struct dentry {
	char filename[14];
	short int f_inode_n;
};

struct files{
	int inode_n;
	unsigned long long read_pointer;
	unsigned long long write_pointer;

	files(int n, unsigned long long rp, unsigned long long wp) {
		inode_n = n;
		read_pointer = rp;
		write_pointer = wp;
	}
};

struct pointer_location {
	int dp;							//dp is which direct pointer to choose
	int sip;						//sip is which address to read out of the available addresses in the target block
	int dip[2];						//dip are which addresses to read out of the available addresses in target blocks
	unsigned long long pointer;		//the actual reading pointer in the block
};

struct memory {
	char *space;

	memory(long long total_size) {
		space = (char *)malloc(total_size);
		bzero(space, total_size);
	}

	~memory() {
		free(space);
	}
};

// global variables
struct memory *disk;
unsigned long long block_size;
unsigned long long data_size;
int wd;								//wd is the inode number of the working directory
vector<struct files> open_files;


//other function prototypes
bool my_mount();
void init(unsigned int file_sys_size, unsigned int block_size_kb, char *name);
bool get_location(struct pointer_location &p);
int getFreeInode();
int getFreeBlock();
unsigned long long write_block_pointers(int ptr_block, unsigned long long offset, int start, char *buf, unsigned long long count);
unsigned long long write_file(struct inode *fd_ptr, char *buf, unsigned long long write_pointer, unsigned long long count, int flag = 0);
unsigned long long read_block_pointers(int ptr_block, unsigned long long offset, int start, char *buf, unsigned long long count);
unsigned long long read_file(struct inode *fd_ptr, char *buf, unsigned long long read_pointer, unsigned long long count);
int breakString(char **list,char *str,const char *delim);
bool parse_path(int &curr, char *file);

bool my_mount() {

	//initializing all inodes initially to be invalid

	struct inode *iptr = (struct inode *)(disk->space + block_size);
	int fnode = 0;

	for(int fnode = 0; fnode<((2*block_size)/sizeof(struct inode)); fnode++) {
		(iptr + fnode)->valid 		= 	false;
		(iptr + fnode)->file 		= 	false;
		(iptr + fnode)->file_size 	= 	0;

		for(int i=0; i<NUMBER_OF_DP; i++){
			(iptr + fnode)->dp[i] = -1;
		}

		(iptr + fnode)->sip = -1;
		(iptr + fnode)->dip = -1;
	}

	//iptr is the first inode, working directory is 0 which is root
	wd = 0;
	iptr->valid = true;													//this inode is valid

	//adding the directory entry "."

	struct dentry temp;
	strcpy(temp.filename, ".");											// . is the current directory
	temp.f_inode_n = 0;
	iptr->file_size += write_file(iptr, (char *)&temp, iptr->file_size, sizeof(temp));

	//adding the directory entry "."

	strcpy(temp.filename, "..");										// .. is the parent directory
	temp.f_inode_n = 0;													//root's parent is root itself
	iptr->file_size += write_file(iptr, (char *)&temp, iptr->file_size, sizeof(temp));

	//mount successfully done
	return true;
}

//function init to be called first to initialize the file system
void init(unsigned int file_sys_size, unsigned int block_size_kb, char *name) {

	//INPUTS : file_sys_size and block_size_kb are in KB, name is the volume name

	unsigned long long total_size;					//total_size is file_sys_size in bytes
	unsigned long long numBlocks;					//number of blocks

	total_size = file_sys_size * KB_TO_BYTES;
	block_size = block_size_kb * KB_TO_BYTES;		//block_size is block_size_kb in bytes

	//each block contains index of the next free block
	data_size = block_size - sizeof(int);			

	if(block_size < MIN_BLOCK_SIZE) {
		perror("Block size is too small\n");
		exit(1);
	}

	numBlocks = total_size/block_size;				//calculating number of blocks
	if(numBlocks > INT_MAX) {
		perror("Number of blocks greater than INT_MAX\n");
		exit(1);
	}

	disk = new memory(block_size * numBlocks);		//creating an instance of memory

	// super block structure pointer points to the starting of the disk space
	struct superblock *sb = (struct superblock *)disk->space;

	//storing details in superblock

	memcpy(sb->volume_name, name, min(sizeof(sb->volume_name), strlen(name)));		//volume name
	sb->block_size = block_size;													//block size
	sb->total_size = block_size * numBlocks;										//total size
	sb->first_free_block = START_DATA_BLOCK;										//start data block is 3

	for(int i=START_DATA_BLOCK; i<numBlocks-1; i++) {

		struct datablock db(disk->space + i*block_size, block_size);				//initialize datablock
		db.set_pointer(i+1);														//set i+1 as the next block
	}


	//last data block doesn't have a next block
	struct datablock db(disk->space + (numBlocks-1)*block_size, block_size);
	db.set_pointer(-1);

	//mounting the file system
	if(!my_mount()) {
		perror("Mount Failed\n");
		exit(1);
	}
}

bool get_location(struct pointer_location &p) {
	p.dp = 0;
	p.sip = 0;
	p.dip[0] = 0;
	p.dip[1] = 0;

	// get dp pointer value
	// p.dp = p.pointer/block_size;
	p.dp = p.pointer/data_size;

	if(p.dp >= NUMBER_OF_DP){ 

		//if p.dp exceeds NUMBER_OF_DP, then p.sip has to be set

		p.sip = p.dp - NUMBER_OF_DP;
	}
	
	// bug 3 corrected : better declare a global variable
	// unsigned long long n_entries = block_size/sizeof(int);
	unsigned long long n_entries = data_size/sizeof(int);

	if(p.sip >= n_entries) {

		//p.sip exceeds, p.dip has to be used

		// p.pointer -= (NUMBER_OF_DP + n_entries) * block_size;
		p.pointer -= (NUMBER_OF_DP + n_entries) * data_size;

		//calculating p.dip[0]
		// p.dip[0] = p.pointer/(n_entries * block_size);
		p.dip[0] = p.pointer/(n_entries * data_size);
		if(p.dip[0] > n_entries) {
			perror("Read/Write pointer error, pointer out of bounds!");
			return false;
		}

		// calculating p.dip[1] and p.pointer
		// p.pointer = p.pointer%(n_entries * block_size);
		// p.dip[1] = p.pointer/block_size;
		// p.pointer = p.pointer%block_size;

		p.pointer = p.pointer%(n_entries * data_size);
		p.dip[1] = p.pointer/data_size;
		p.pointer = p.pointer%data_size;
	}
	else{ 
	
		//otherwise just set p.pointer to the following value
		p.pointer = p.pointer%data_size;
	}

	return true;
}

int getFreeInode() {
	
	//returns the next free available inode

	struct inode *iptr = (struct inode *)(disk->space + block_size);
	int fnode = 0;

	for(int fnode=0; fnode<((2*block_size)/sizeof(struct inode)); fnode++) {
		if(!(iptr + fnode)->valid) {
			return fnode;
		}
	}

	return -1;
}

//returns the first free block from info of superblock and updates the first free block as the block by the next pointer
int getFreeBlock(){

	//get the first free block from the information stored in super block
	struct superblock *sb = (struct superblock *)disk->space;
	int curr_free = sb->first_free_block;

	//No more free block available!
	if(curr_free == -1)
		return -1;

	datablock db(disk->space + curr_free*block_size, block_size);
	int next_free = db.get_pointer();

	//update the superblock's first free block
	sb->first_free_block = next_free;
	return curr_free;
}

unsigned long long write_block_pointers(int ptr_block, unsigned long long offset, int start, char *buf, unsigned long long count) {

	//ptr_block is the block which holds pointers to other blocks
	//offset is the point from where writing will be started
	//start is the starting block index
	//buf is the buffer to write
	//count is the number of characters to write

	int *block_addr = (int *)(disk->space + ptr_block*block_size);
	unsigned long long n_entries = data_size/sizeof(int);

	unsigned long long bytes_written = 0;

	for(int i=start; i<n_entries; i++) {

		//blk is the index of the block on which we will write

		int blk = *(block_addr + i);

		if(blk == -1) {

			//getting free block in case the index is invalid

			blk = getFreeBlock();

			if(blk < 0) {
				perror("No more space available\n");
				return bytes_written;
			}


			*(block_addr + i) = blk;
		}

		//writing data

		datablock db(disk->space + blk*block_size, block_size);
		bytes_written += db.set_data(buf + bytes_written, count - bytes_written, offset);

		if(bytes_written == count) {
			return bytes_written;
		}

		//resetting offset
		offset = 0;
	}

	return bytes_written;

}

unsigned long long write_file(struct inode *fd_ptr, char *buf, unsigned long long write_pointer, unsigned long long count, int flag) {

	//fd_ptr is the inode pointer in consideration
	//buf is the data to be written
	//write pointer is the location from where write operation has to be started
	//count is the number of bytes to write

	struct pointer_location p;
	p.pointer = write_pointer;
	get_location(p);

	unsigned long long bytes_written = 0;

	for(int i=p.dp; i<NUMBER_OF_DP; i++) {

		if(fd_ptr->dp[i] == -1 && count > bytes_written){
		
			//dp[i] currently doesn't point to any block

			//find a free block
			int fb = getFreeBlock();
			if(fb < 0) {
				perror("No more space available\n");
				return bytes_written;
			}

			//dp[i] stores index of the free block
			fd_ptr->dp[i] = fb;
			datablock db(disk->space + fb*block_size, block_size);

			bytes_written += db.set_data(buf + bytes_written, count - bytes_written);

			if(bytes_written == count) {
				return bytes_written;
			}
		}
		else{

			//dp[i] points to some block

			if(flag == 1){
				//fd_ptr->dp[i] represents a free block

				int blockNumber = fd_ptr->dp[i];

				// super block structure pointer points to the starting of the disk space
				struct superblock *sb = (struct superblock *)disk->space;

				datablock db(disk->space + blockNumber*block_size, block_size);
				db.eraseDataBlock();
				db.set_pointer(sb->first_free_block);
				sb->first_free_block = blockNumber;
			}
			
			datablock db(disk->space + fd_ptr->dp[i]*block_size, block_size);
			bytes_written += db.set_data(buf + bytes_written, count - bytes_written, p.pointer);
			p.pointer = 0;

			if(bytes_written == count) {
				return bytes_written;
			}	
		}
	}

	//all dp blocks exhausted, moving to fill sip blocks

	int sip_addr = fd_ptr->sip;

	if(sip_addr == -1) {

		//sip_addr is invalid

		//getting a free block
		sip_addr = getFreeBlock();

		if(sip_addr < 0) {
			perror("No more space available\n");
			return bytes_written;
		}

		//assigning it to sip
		fd_ptr->sip = sip_addr;

		//initially all entries will be invalid
		int *block_addr = (int *)(disk->space + block_size * sip_addr);
		for(int i=0; i<data_size/sizeof(int); i++) {
			*(block_addr + i) = -1;
		}

		p.sip = 0;
		p.pointer = 0;
	}
	
	//writing data on blocks indexed in the block pointed by sip_addr

	bytes_written += write_block_pointers(sip_addr, p.pointer, p.sip, buf + bytes_written, count - bytes_written);
	if(bytes_written == count) {
		return bytes_written;
	}

	//bytes are still left to write, as bytes_written isn't equal to count

	int dip_addr = fd_ptr->dip;

	if(dip_addr == -1) {
	
		//dip_addr is invalid, we need to assign a free block

		dip_addr = getFreeBlock();

		if(dip_addr < 0) {
			perror("No more space available\n");
			return bytes_written;
		}

		//initially all entries will be invalid
		fd_ptr->dip = dip_addr;
		int *block_addr = (int *)(disk->space + block_size * dip_addr);
		for(int i=0; i<data_size/sizeof(int); i++) {
			*(block_addr + i) = -1;
		}

		p.dip[0] = 0;
		p.dip[1] = 0;
		p.pointer = 0;
	}

	int *block_addr = (int *)(disk->space + block_size * dip_addr);
	for(int i=p.dip[0]; i<data_size/sizeof(int); i++) {
		int blk = *(block_addr + i);

		// bug 2 corrected
		if(blk == -1) {
			blk = getFreeBlock();
			*(block_addr + i) = blk;
		}

		bytes_written += write_block_pointers(blk, p.pointer, p.dip[1], buf + bytes_written, count - bytes_written);

		if(bytes_written == count) {
			return bytes_written;
		}

		p.dip[1] = 0;
		p.pointer = 0;
	}

	return bytes_written;

}

unsigned long long my_write(int fd, char *buf, unsigned long long count) {

	int fnode = open_files[fd].inode_n;
	unsigned long long wp = open_files[fd].write_pointer;

	struct inode* fd_ptr = (struct inode *)(disk->space + block_size + fnode*sizeof(struct inode));

	unsigned long long bytes_written = 0;
	
	bytes_written = write_file(fd_ptr, buf, wp, count);

	fd_ptr->file_size += bytes_written;
	open_files[fd].write_pointer += bytes_written;

	return bytes_written;

}

unsigned long long read_block_pointers(int ptr_block, unsigned long long offset, int start, char *buf, unsigned long long count) {

	//ptr_block is the block which holds pointers to other blocks
	//offset is the point from where reading will be started
	//start is the starting block index
	//buf is the buffer to which data will be written after reading from data block
	//count is the number of characters to read

	int *block_addr = (int *)(disk->space + ptr_block*block_size);
	unsigned long long n_entries = data_size/sizeof(int);

	unsigned long long bytes_read = 0;

	for(int i=start; i<n_entries; i++) {
		int blk = *(block_addr + i);

		if(blk == -1) return bytes_read;

		datablock db(disk->space + blk*block_size, block_size);
		bytes_read += db.get_data(buf + bytes_read, count - bytes_read, offset);

		if(bytes_read == count) {
			return bytes_read;
		}

		offset = 0;
	}

	return bytes_read;
}

unsigned long long read_file(struct inode *fd_ptr, char *buf, unsigned long long read_pointer, unsigned long long count) {
	
	struct pointer_location p;
	p.pointer = read_pointer;
	get_location(p);

	unsigned long long bytes_read = 0;

	for(int i=p.dp; i<NUMBER_OF_DP; i++) {
		if(fd_ptr->dp[i] != -1 && count > bytes_read) {

			//block exists and bytes are left to read

			datablock db(disk->space + fd_ptr->dp[i]*block_size, block_size);

			//read bytes from data block
			bytes_read += db.get_data(buf + bytes_read, count - bytes_read, p.pointer);
			p.pointer = 0;

			if(bytes_read == count) 
				return bytes_read;
		}
		else 
			return bytes_read;
	}

	int sip_addr = fd_ptr->sip;
	
	//if sip_addr is invalid, then nothing to read
	if(sip_addr == -1) 
		return bytes_read;

	bytes_read += read_block_pointers(sip_addr, p.pointer, p.sip, buf + bytes_read, count - bytes_read);
	if(bytes_read == count) {
		return bytes_read;
	}

	int dip_addr = fd_ptr->dip;

	//if dip_addr is invalid, then nothing more to read
	if(dip_addr == -1) return bytes_read;

	int *block_addr = (int *)(disk->space + block_size*dip_addr);
	for(int i=p.dip[0]; i<data_size/sizeof(int); i++) {
		int blk = *(block_addr + i);

		// bug 4 corrected
		if(blk == -1) {
			return bytes_read;
		}

		bytes_read += read_block_pointers(blk, p.pointer, p.dip[1], buf + bytes_read, count - bytes_read);
		if(bytes_read == count) {
			return bytes_read;
		}
	}

	return bytes_read;
}

unsigned long long my_read(int fd, char *buf, unsigned long long count) {

	int fnode = open_files[fd].inode_n;
	unsigned long long rp = open_files[fd].read_pointer;

	struct inode* fd_ptr = (struct inode *)(disk->space + block_size + fnode*sizeof(struct inode));

	unsigned long long bytes_read = 0;

	bytes_read = read_file(fd_ptr, buf, rp, count);

	open_files[fd].read_pointer += bytes_read;

	return bytes_read;
}

int breakString(char **list,char *str,const char *delim) {
	int i=0;
	list[i]=strtok(str,delim);
	while(list[i]!=NULL){
		i++;
		list[i]=strtok(NULL,delim);
	}
	return i;
}

bool parse_path(int &curr, char *file) {

	//getting inode corresponding to the current directory
	struct inode *curr_inode = (struct inode *)(disk->space + block_size + curr*sizeof(struct inode));
	
	//intermediate inodes should be valid directories
	if(curr_inode->file == true || curr_inode->valid == false) return false;

	char *buf = (char *)malloc(curr_inode->file_size);

	//read directory in the buffer
	read_file(curr_inode, buf, 0, curr_inode->file_size);

	struct dentry *d = (struct dentry *)buf;

	for(int i=0; i<block_size/sizeof(dentry); i++) {
		
		//finding file in the directory

		if(!strcmp((d+i)->filename, file)) {

			//setting curr val to inode of the file found
			curr = (d+i)->f_inode_n;
			return true;
		}
	}

	//file not found in the directory
	return false;
}

int my_close(int fd) {

	if(fd >= open_files.size()){
		return -1;
	}

	//inode becomes invalid and read_pointer of the file is reset to 0
	open_files[fd].inode_n = -1;
	open_files[fd].read_pointer = 0;

	return 0;
}

int my_open(const char *filename) {

	//takes filename as input which is the path
	
	char *args[MAX_PATH];
	char path[MAX_PATH];

	strcpy(path, filename);

	int n = breakString(args, path, "/\n");				//args[n] = NULL

	int curr;
	if(filename[0] == '/') 
		curr = 0;										//current directory is root
	else
		curr = wd;										//current directory same as working directory

	for(int i=0; i<n-1; i++) {

		//check if the intermediate names are valid directories

		if(!parse_path(curr, (char *)args[i])) {
			perror("Wrong Path\n");
			return -1;
		}
	}

	//filename is too long
	if(strlen(args[n-1])>13) {
		printf("Name is too long");
		return -1;
	}

	//curr is now inode of the directory in which the file is residing

	struct inode *curr_inode = (struct inode *)(disk->space + block_size + curr*sizeof(struct inode));
	char *buf = (char *)malloc(curr_inode->file_size);

	//reading current directory contents in buf
	read_file(curr_inode, buf, 0, curr_inode->file_size);
	
	//searching for file in the directory
	struct dentry *d = (struct dentry *)buf;
	for(int i=0; i<curr_inode->file_size/sizeof(dentry); i++) {
		// previously d was not increased
		if(!strcmp((d+i)->filename, args[n-1])) {

			//updating the open files vector

			// bug 7 urgent bug
			struct inode *f_inode = (struct inode *)(disk->space + block_size + ((d+i)->f_inode_n)*sizeof(struct inode));

			struct files open_file((d+i)->f_inode_n, 0, f_inode->file_size);
			open_files.push_back(open_file);
			return open_files.size()-1;
		}
	}

	//file not found

	int fnode = getFreeInode();						//get free inode
	
	//updating directory
	struct dentry file_data;						
	strcpy(file_data.filename, args[n-1]);
	file_data.f_inode_n = fnode;
	
	//writing in current directory
	curr_inode->file_size += write_file(curr_inode, (char *)&file_data, curr_inode->file_size, sizeof(file_data));

	struct inode *f_inode = (struct inode *)(disk->space + block_size + fnode * sizeof(struct inode));
	f_inode->valid = true;					//the inode is now valid
	f_inode->file = true;					//it is a file
	f_inode->file_size = 0;					//file_size is 0

	//initializing inode to default values

	for(int i=0; i<NUMBER_OF_DP; i++) {
		f_inode->dp[i] = -1;				
	}

	f_inode->sip = -1;
	f_inode->dip = -1;

	//updating the open files vector

	struct files open_file(fnode, 0, 0);
	open_files.push_back(open_file);

	return open_files.size()-1;
}

unsigned long long my_cat(int fd) {

	char *buf;

	int fnode = open_files[fd].inode_n;

	struct inode* fd_ptr = (struct inode *)(disk->space + block_size + fnode*sizeof(struct inode));

	unsigned long long bytes_read = 0;

	buf = (char *)malloc(fd_ptr->file_size);
	bytes_read = read_file(fd_ptr, buf, 0, fd_ptr->file_size);

	for(int i=0; i<(fd_ptr->file_size); i++) {
		printf("%c",buf[i]);
	}

	return bytes_read;
}

bool my_copy(int fd, const char *path) {
	
	int fnode = open_files[fd].inode_n;
	if(fnode < 0) return false;

	int f = open(path, O_RDONLY);
	if(f < 0) return false;

	int FSIZE = lseek(f, 0, SEEK_END);
	lseek(f, 0, SEEK_SET);

	char *buf = (char *)malloc(FSIZE);
	read(f, buf, FSIZE);
	
	struct inode *fd_ptr = (struct inode *)(disk->space + block_size + fnode*sizeof(struct inode));

	fd_ptr->file_size = write_file(fd_ptr, buf, 0, FSIZE);


	return true;
}

bool my_copy(const char *path, int fd) {

	int f = open(path, O_WRONLY|O_CREAT|O_TRUNC,S_IRWXU);
	if(f < 0) return false;

	int fnode = open_files[fd].inode_n;
	if(fnode < 0) return false;

	struct inode *fd_ptr = (struct inode *)(disk->space + block_size + fnode*sizeof(struct inode));

	char *buf = (char *)malloc(fd_ptr->file_size);

	read_file(fd_ptr, buf, 0, fd_ptr->file_size);
	write(f, buf, fd_ptr->file_size);

	return true;
}

int my_mkdir(const char *filename){

	//takes filename as input which is the path
	
	char *args[MAX_PATH];
	char path[MAX_PATH];

	strcpy(path, filename);

	int n = breakString(args, path, "/\n");				//args[n] = NULL

	int curr;
	if(filename[0] == '/') 
		curr = 0;										//current directory is root
	else
		curr = wd;										//current directory same as working directory

	// for(int i=0;i<n;i++){
	// 	cout<<args[i]<<'\n';
	// }
	// cout<<wd<<'\n';
	// fflush(NULL);

	for(int i=0; i<n-1; i++) {

		//check if the intermediate names are valid directories

		if(!parse_path(curr, (char *)args[i])) {
			perror("Wrong Path\n");
			return -1;
		}
	}

	//filename is too long
	if(strlen(args[n-1])>13) {
		printf("Name is too long");
		return -1;
	}

	//curr is now inode of the directory in which the file is residing

	struct inode *curr_inode = (struct inode *)(disk->space + block_size + curr*sizeof(struct inode));
	char *buf = (char *)malloc(curr_inode->file_size);

	//reading current directory contents in buf
	read_file(curr_inode, buf, 0, curr_inode->file_size);
	
	//searching for file in the directory
	struct dentry *d = (struct dentry *)buf;
	for(int i=0; i<curr_inode->file_size/sizeof(dentry); i++) {
		// previously d was not increased
		if(!strcmp((d+i)->filename, args[n-1])) {
			return 0;
		}
	}

	//file not found

	int fnode = getFreeInode();						//get free inode
	
	//updating directory
	struct dentry file_data;						
	strcpy(file_data.filename, args[n-1]);
	file_data.f_inode_n = fnode;
	
	//writing in current directory
	curr_inode->file_size += write_file(curr_inode, (char *)&file_data, curr_inode->file_size, sizeof(file_data));

	struct inode *f_inode = (struct inode *)(disk->space + block_size + fnode * sizeof(struct inode));
	f_inode->valid = true;					//the inode is now valid
	f_inode->file = false;					//it is a directory
	f_inode->file_size = 0;					//file_size is 0

	//initializing inode to default values

	for(int i=0; i<NUMBER_OF_DP; i++) {
		f_inode->dp[i] = -1;				
	}

	f_inode->sip = -1;
	f_inode->dip = -1;


	//adding the directory entry "."

	struct dentry temp;
	strcpy(temp.filename, ".");											// . is the current directory
	temp.f_inode_n = fnode;
	f_inode->file_size += write_file(f_inode, (char *)&temp, f_inode->file_size, sizeof(temp));

	//adding the directory entry "."

	strcpy(temp.filename, "..");										// .. is the parent directory
	temp.f_inode_n = curr;													//root's parent is root itself
	f_inode->file_size += write_file(f_inode, (char *)&temp, f_inode->file_size, sizeof(temp));



	return 0;
}

int my_rmdir(const char *filename, int recurseCount){

	if(recurseCount == 0){
		cout<< "Deleted Directories are : \n";
	}


	string inputpath(filename);

	//takes filename as input which is the path
	
	char *args[MAX_PATH];
	char path[MAX_PATH];

	strcpy(path, filename);

	int n = breakString(args, path, "/\n");				//args[n] = NULL

	int curr;
	if(filename[0] == '/') 
		curr = 0;										//current directory is root
	else
		curr = wd;										//current directory same as working directory

	for(int i=0; i<n-1; i++) {

		//check if the intermediate names are valid directories

		if(!parse_path(curr, (char *)args[i])) {
			printf("Wrong Path\n");
			return -1;
		}
	}

	//filename is too long
	if(strlen(args[n-1])>13) {
		printf("Name is too long");
		return -1;
	}

	//curr is now inode of the directory in which the file is residing

	struct inode *curr_inode = (struct inode *)(disk->space + block_size + curr*sizeof(struct inode));
	char *buf = (char *)malloc(curr_inode->file_size);

	//reading current directory contents in buf
	read_file(curr_inode, buf, 0, curr_inode->file_size);
	
	//searching for file in the directory
	struct dentry *d = (struct dentry *)buf;
	for(int i=0; i<curr_inode->file_size/sizeof(dentry); i++) {
		// previously d was not increased
		if(!strcmp((d+i)->filename, args[n-1])) {

			//remove the directory whose inode is at index (d+i)->f_inode_n

			struct inode *dir_inode = (struct inode *)(disk->space + block_size + ((d+i)->f_inode_n) * sizeof(struct inode));

			
			char *buf = (char *)malloc(dir_inode->file_size);
			read_file(dir_inode, buf, 0, dir_inode->file_size);

			//cout<<"qwe : \n";

			struct dentry *dire = (struct dentry *)buf;

			for(int t = 0; t<dir_inode->file_size/sizeof(dentry);t++){
				struct inode *eachentry = (struct inode *)(disk->space + block_size + ((dire+t)->f_inode_n) * sizeof(struct inode));

				if(eachentry->file == true){
					string g = inputpath+'/'+(dire+t)->filename;

					char *fileEmptyBuffer = (char*)malloc(eachentry->file_size);
					bzero(fileEmptyBuffer,eachentry->file_size);
					write_file(eachentry, fileEmptyBuffer, 0, eachentry->file_size, 1);
					free(fileEmptyBuffer);
					eachentry->valid = false;
					//cout<<g<<'\n';
					//remove_file()
				}
				else{
					//cout<<inputpath+'/'+(dire+t)->filename<<'\n';

					string g = inputpath+'/'+(dire+t)->filename;
					cout<<g<<'\n';

					if(strcmp((dire+t)->filename,".")!=0 && strcmp((dire+t)->filename,"..")!=0){
						my_rmdir(g.c_str(),recurseCount+1);
					}
				}

				//freeing the inode
				//eachentry->valid = false;
				//eachentry->file = false;
				//eachentry->file_size = 0;

				//cout<<eachentry->file<<'\n';
			}
			//cout<<"qazwsxedc\n";
			//cout<<(d+i)->filename<<endl;

			char *buffer = (char *)malloc(dir_inode->file_size);
			bzero(buffer,dir_inode->file_size);
			write_file(dir_inode, buffer, 0, dir_inode->file_size, 1);
			free(buffer);

			dir_inode->valid = false;
			dir_inode->file_size = 0;

			//removing directory from the parent directory list
			if(recurseCount == 0){
				int c = read_file(curr_inode, buf, sizeof(dentry)*(i+1), curr_inode->file_size);
				write_file(curr_inode, buf, sizeof(dentry)*i, c);
			}
			//cout<<'\n';

			// bool valid;										//valid means it has some file associated with it
			// bool file;										//true if it represents a file
			// unsigned long long file_size;					//stores size of file
			// int dp[NUMBER_OF_DP];							//direct pointers
			// int sip;										//singly indirect pointers
			// int dip;										//doubly indirect pointers

			free(buf);
			return 0;
		}
	}

	return -1;
}

int my_chdir(const char *filename){

	//takes filename as input which is the path
	
	char *args[MAX_PATH];
	char path[MAX_PATH];

	strcpy(path, filename);

	int n = breakString(args, path, "/\n");				//args[n] = NULL

	if(n==0){
		wd = 0;
		return 0;
	}

	int curr;
	if(filename[0] == '/') 
		curr = 0;										//current directory is root
	else
		curr = wd;										//current directory same as working directory

	for(int i=0; i<n-1; i++) {

		//check if the intermediate names are valid directories

		if(!parse_path(curr, (char *)args[i])) {
			perror("Wrong Path\n");
			return -1;
		}
	}

	//filename is too long
	if(strlen(args[n-1])>13) {
		printf("Name is too long");
		return -1;
	}

	//curr is now inode of the directory in which the file is residing

	struct inode *curr_inode = (struct inode *)(disk->space + block_size + curr*sizeof(struct inode));
	char *buf = (char *)malloc(curr_inode->file_size);

	//reading current directory contents in buf
	read_file(curr_inode, buf, 0, curr_inode->file_size);
	
	//searching for file in the directory
	struct dentry *d = (struct dentry *)buf;
	for(int i=0; i<curr_inode->file_size/sizeof(dentry); i++) {
		// previously d was not increased
		if(!strcmp((d+i)->filename, args[n-1])) {

			wd = (d+i)->f_inode_n;

			//cout<<wd<<'\n';

			return 0;
		}
	}

	return -1;
}


/*

this is the file fat.cpp

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
*/