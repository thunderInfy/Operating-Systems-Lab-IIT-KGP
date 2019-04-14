#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <climits>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
using namespace std;

#define MIN_BLOCK_SIZE 32							//32 is arbitrary
#define KB_TO_BYTES 1024
#define START_DATA_BLOCK 3
#define NUMBER_OF_DP 5
#define MAX_PATH 100
#define TYPE_DIR false
#define TYPE_FILE true
#define VALID true
#define INVALID false

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

//API function prototypes
int my_open(const char *filename);
int my_close(int fd);
unsigned long long my_read(int fd, char *buf, unsigned long long count);
unsigned long long my_write(int fd, char *buf, unsigned long long count);
bool my_copy(int fd, const char *path);
bool my_copy(const char *path, int fd);
unsigned long long my_cat(int fd, char *buf);


//other function prototypes
bool my_mount();
void init(unsigned int file_sys_size, unsigned int block_size_kb, char *name);
bool get_location(struct pointer_location &p);
int getFreeInode();
int getFreeBlock();
unsigned long long write_block_pointers(int ptr_block, unsigned long long offset, int start, char *buf, unsigned long long count);
unsigned long long write_file(struct inode *fd_ptr, char *buf, unsigned long long write_pointer, unsigned long long count);
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

	//get dp pointer value
	p.dp = p.pointer/block_size;

	if(p.dp >= NUMBER_OF_DP){ 

		//if p.dp exceeds NUMBER_OF_DP, then p.sip has to be set

		p.sip = p.dp - NUMBER_OF_DP;
	}
	
	unsigned long long n_entries = block_size/sizeof(int);

	if(p.sip >= n_entries) {

		//p.sip exceeds, p.dip has to be used

		p.pointer -= (NUMBER_OF_DP + n_entries) * block_size;

		//calculating p.dip[0]
		p.dip[0] = p.pointer/(n_entries * block_size);
		if(p.dip[0] > n_entries) {
			perror("Read/Write pointer error, pointer out of bounds!");
			return false;
		}

		//calculating p.dip[1] and p.pointer
		p.pointer = p.pointer%(n_entries * block_size);
		p.dip[1] = p.pointer/block_size;
		p.pointer = p.pointer%block_size;
	}
	else{ 
	
		//otherwise just set p.pointer to the following value
		p.pointer = p.pointer%block_size;
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

unsigned long long write_file(struct inode *fd_ptr, char *buf, unsigned long long write_pointer, unsigned long long count) {

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
	if(dip_addr == -1) return bytes_read;

	int *block_addr = (int *)(disk->space + block_size*dip_addr);
	for(int i=p.dip[0]; i<data_size/sizeof(int); i++) {
		int blk = *(block_addr + i);

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
	if(curr_inode->file == true || curr_inode->valid == false) return -1;

	char *buf = (char *)malloc(curr_inode->file_size);

	read_file(curr_inode, buf, 0, curr_inode->file_size);

	struct dentry *d = (struct dentry *)buf;

	for(int i=0; i<block_size/sizeof(dentry); i++) {
		if(!strcmp(d->filename, file)) {
			curr = d->f_inode_n;
			return true;
		}
	}
	return false;
}

int my_close(int fd) {
	open_files[fd].inode_n = -1;
	open_files[fd].read_pointer = 0;
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
		printf("Too long file name");
		return -1;
	}

	struct inode *curr_inode = (struct inode *)(disk->space + block_size + curr*sizeof(struct inode));
	char *buf = (char *)malloc(curr_inode->file_size);
	read_file(curr_inode, buf, 0, curr_inode->file_size);
	
	struct dentry *d = (struct dentry *)buf;
	for(int i=0; i<sizeof(buf)/sizeof(dentry); i++) {
		if(!strcmp(d->filename, args[n-1])) {

			struct files open_file(d->f_inode_n, 0, 0);
			open_files.push_back(open_file);
			return open_files.size()-1;
		}
	}

	int fnode = getFreeInode();
	struct dentry file_data;
	strcpy(file_data.filename, args[n-1]);
	file_data.f_inode_n = fnode;
	curr_inode->file_size += write_file(curr_inode, (char *)&file_data, curr_inode->file_size, sizeof(file_data));

	struct inode *f_inode = (struct inode *)(disk->space + block_size + fnode * sizeof(struct inode));
	f_inode->valid = true;
	f_inode->file = true;
	f_inode->file_size = 0;

	for(int i=0; i<NUMBER_OF_DP; i++) {
		f_inode->dp[i] = -1;
	}

	f_inode->sip = -1;
	f_inode->dip = -1;

	struct files open_file(fnode, 0, 0);
	open_files.push_back(open_file);

	return open_files.size()-1;
}

unsigned long long my_cat(int fd, char *buf) {

	int fnode = open_files[fd].inode_n;
	unsigned long long rp = 0;

	struct inode* fd_ptr = (struct inode *)(disk->space + block_size + fnode*sizeof(struct inode));

	unsigned long long bytes_read = 0;

	buf = (char *)malloc(fd_ptr->file_size);
	bytes_read = read_file(fd_ptr, buf, rp, fd_ptr->file_size);

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
	my_write(fd, buf, FSIZE);

	return true;
}

bool my_copy(const char *path, int fd) {

	int f = open(path, O_WRONLY|O_CREAT|O_TRUNC);
	if(f < 0) return false;

	int fnode = open_files[fd].inode_n;
	if(fnode < 0) return false;

	struct inode *fd_ptr = (struct inode *)(disk->space + block_size + fnode*sizeof(struct inode));

	char *buf = (char *)malloc(fd_ptr->file_size);
	my_read(fd, buf, fd_ptr->file_size);
	write(f, buf, fd_ptr->file_size);

	return true;
}

int main() {
	return 0;
}