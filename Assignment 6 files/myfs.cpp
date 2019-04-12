#include <bits/stdc++.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
using namespace std;

#define MIN_SIZE 32
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
	bool valid;
	bool file;
	unsigned long long file_size;

	int dp[NUMBER_OF_DP];
	int sip;
	int dip;
};

struct datablock {
	char *data;
	int *next;
	unsigned long long dsize;

	datablock(char *ptr, unsigned long long bsize) {
		data = ptr;
		dsize = bsize - sizeof(int);
		next = (int *)(ptr + dsize);
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
		
		len = (len <= (dsize - offset))?len:(dsize - offset);
		memcpy(data + offset, val, len);
		return len;
	}

	unsigned long long get_data(char *val, unsigned long long len, unsigned long long offset = 0) {

		len = (len <= (dsize - offset))?len:(dsize - offset);
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
	unsigned long long pointer;
	int dp;
	int sip;
	int dip[2];
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
int wd;
vector<struct files> open_files;


bool my_mount() {

	wd = 0;
	struct inode *wd_inode = (struct inode *)(disk->space + block_size);
	wd_inode->valid = true;
	wd_inode->file = true;
	wd_inode->file_size = 0;

	for(int i=0; i<NUMBER_OF_DP; i++) {
		wd_inode->dp[i] = -1;
	}
	wd_inode->sip = -1;
	wd_inode->dip = -1;

	struct files open_file(wd, 0, wd_inode->file_size);
	open_files.push_back(open_file);

	return true;
}

void init(unsigned int file_sys_size, unsigned int block_size_kb, char *name) {

	unsigned long long total_size = file_sys_size * KB_TO_BYTES;
	block_size = block_size_kb * KB_TO_BYTES;
	data_size = block_size - sizeof(int);

	if(block_size < MIN_SIZE) {
		printf("Block size is too small\n");
		exit(1);
	}

	unsigned long long numBlocks = total_size/block_size;
	if(numBlocks > 1<<16-1) {
		printf("Too many blocks\n");
		exit(1);
	}

	disk = new memory(block_size * numBlocks);

	// initialize super block
	struct superblock *sb = (struct superblock *)disk->space;
	memcpy(sb->volume_name, name, min(sizeof(sb->volume_name), strlen(name)));
	sb->block_size = block_size;
	sb->total_size = block_size * numBlocks;
	sb->first_free_block = START_DATA_BLOCK;

	for(int i=START_DATA_BLOCK; i<numBlocks-1; i++) {

		struct datablock db(disk->space + i*block_size, block_size);
		db.set_pointer(i+1);
	}

	struct datablock db(disk->space + (numBlocks-1)*block_size, block_size);
	db.set_pointer(-1);

	if(!my_mount()) {
		printf("Mount Failed\n");
		exit(1);
	}
}

bool get_location(struct pointer_location &p) {
	p.dp = 0;
	p.sip = 0;
	p.dip[0] = 0;
	p.dip[1] = 0;

	unsigned long long n_entries = block_size/sizeof(int);
	p.dp = p.pointer/block_size;
	if(p.dp >= NUMBER_OF_DP) p.sip = p.dp - NUMBER_OF_DP;

	if(p.sip >= n_entries) {
		p.pointer -= (NUMBER_OF_DP + n_entries) * block_size;

		p.dip[0] = p.pointer/(n_entries * block_size);
		if(p.dip[0] > n_entries) {
			printf("Read/Write pointer error");
			return false;
		}

		p.pointer = p.pointer%(n_entries * block_size);
		p.dip[1] = p.pointer/block_size;
		p.pointer = p.pointer%block_size;
	}
	else p.pointer = p.pointer%block_size;
	return true;
}

int getFreeInode() {
	struct inode *iptr = (struct inode *)(disk->space + block_size);
	int fnode = 0;

	for(int fnode=0; fnode<2*block_size/sizeof(inode); fnode++) {
		if(!iptr->valid) {
			return fnode;
		}
	}

	return -1;
}

int getFreeBlock() {
	struct superblock *sb = (struct superblock *)disk->space;
	int curr_free = sb->first_free_block;

	if(curr_free == -1) return -1;

	datablock db(disk->space + curr_free*block_size, block_size);
	int next_free = db.get_pointer();

	sb->first_free_block = next_free;
	return curr_free;
}

unsigned long long write_block_pointers(int ptr_block, unsigned long long offset, int start, char *buf, unsigned long long count) {

	int *block_addr = (int *)(disk->space + ptr_block*block_size);
	unsigned long long n_entries = data_size/sizeof(int);

	unsigned long long bytes_written = 0;

	for(int i=start; i<n_entries; i++) {
		int blk = *(block_addr + i);

		if(blk == -1) {
			blk = getFreeBlock();
			*(block_addr + i) = blk;
		}

		datablock db(disk->space + blk*block_size, block_size);
		bytes_written += db.set_data(buf + bytes_written, count - bytes_written, offset);

		if(bytes_written == count) {
			return bytes_written;
		}

		offset = 0;
	}

	return bytes_written;

}

unsigned long long write_file(struct inode *fd_ptr, char *buf, unsigned long long write_pointer, unsigned long long count) {
	struct pointer_location p;
	p.pointer = write_pointer;
	get_location(p);

	unsigned long long bytes_written = 0;

	for(int i=p.dp; i<NUMBER_OF_DP; i++) {
		if(fd_ptr->dp[i] == -1 && count > bytes_written) {
			int fb = getFreeBlock();
			if(fb < 0) {
				printf("No more space available\n");
				return bytes_written;
			}

			fd_ptr->dp[i] = fb;
			datablock db(disk->space + fb*block_size, block_size);

			bytes_written += db.set_data(buf + bytes_written, count - bytes_written);

			if(bytes_written == count) {
				return bytes_written;
			}
		}
		else {
			datablock db(disk->space + fd_ptr->dp[i]*block_size, block_size);
			bytes_written += db.set_data(buf + bytes_written, count - bytes_written, p.pointer);
			p.pointer = 0;

			if(bytes_written == count) {
				return bytes_written;
			}	
		}
	}

	int sip_addr = fd_ptr->sip;

	if(sip_addr == -1) {
		sip_addr = getFreeBlock();
		fd_ptr->sip = sip_addr;
		int *block_addr = (int *)(disk->space + block_size * sip_addr);
		for(int i=0; i<data_size/sizeof(int); i++) {
			*(block_addr + i) = -1;
		}
		p.sip = 0;
		p.pointer = 0;
	}
	bytes_written += write_block_pointers(sip_addr, p.pointer, p.sip, buf + bytes_written, count - bytes_written);
	if(bytes_written == count) {
		return bytes_written;
	}

	int dip_addr = fd_ptr->dip;
	if(dip_addr == -1) {
		dip_addr = getFreeBlock();
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

	struct inode* fd_ptr = (struct inode *)(disk->space + block_size + fnode*sizeof(fnode));

	unsigned long long bytes_written = 0;
	
	bytes_written = write_file(fd_ptr, buf, wp, count);

	fd_ptr->file_size += bytes_written;
	open_files[fd].write_pointer += bytes_written;

	return bytes_written;

}

unsigned long long read_block_pointers(int ptr_block, unsigned long long offset, int start, char *buf, unsigned long long count) {

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
			datablock db(disk->space + fd_ptr->dp[i]*block_size, block_size);

			bytes_read += db.get_data(buf + bytes_read, count - bytes_read, p.pointer);
			p.pointer = 0;

			if(bytes_read == count) 
				return bytes_read;
		}
		else return bytes_read;
	}

	int sip_addr = fd_ptr->sip;
	if(sip_addr == -1) return bytes_read;

	bytes_read += read_block_pointers(sip_addr, p.pointer, p.sip, buf, count - bytes_read);
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

	struct inode* fd_ptr = (struct inode *)(disk->space + block_size + fnode*sizeof(fnode));

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

	struct inode *curr_inode = (struct inode *)(disk->space + block_size + curr*sizeof(inode));
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
	
	char *args[MAX_PATH], path[MAX_PATH];
	strcpy(path, filename);

	int n = breakString(args, path, "/\n");

	int curr = 0;
	if(filename[0] != '/') curr = wd;

	for(int i=0; i<n-1; i++) {
		if(!parse_path(curr, (char *)args[i])) {
			printf("Wrong Path\n");
			return -1;
		}
	}

	if(strlen(args[n-1])>13) {
		printf("Too long file name");
		return -1;
	}

	struct inode *curr_inode = (struct inode *)(disk->space + block_size + curr*sizeof(inode));
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
	write_file(curr_inode, (char *)&file_data, curr_inode->file_size, sizeof(file_data));

	struct inode *f_inode = (struct inode *)(disk->space + block_size + fnode * sizeof(inode));
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

	struct inode* fd_ptr = (struct inode *)(disk->space + block_size + fnode*sizeof(fnode));

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