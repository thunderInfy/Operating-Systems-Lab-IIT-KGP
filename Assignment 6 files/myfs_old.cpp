#include <bits/stdc++.h>
using namespace std;

#define MIN_SIZE 0
#define KB_TO_BYTES 1024
#define START_DATA_BLOCK 3
#define NUMBER_DP 5
#define MAX_PATH 100
#define TYPE_DIR 'd'
#define TYPE_FILE 'f'
#define INVALID 'i'

struct super_block {
	char volume_name[8];
	unsigned long long block_size;
	unsigned long long total_size;
	int first_free_block;
};

struct inode {
	char type;
	unsigned long long file_size;

	int dp[NUMBER_DP];
	int sip;
	int dip;
};

struct block_pointer {
	unsigned long long rp;
	int dp;
	int sip;
	int dip[2];
};

struct directory_data {
	char filename[14];
	short int f_inode;
};

// open files table
struct open_files {
	int f_inode;
	unsigned long long read_pointer;
	unsigned long long write_pointer;
};
vector<struct open_files> open_files_table;

// inode of working directory
short int wd_inode;

struct memory{
	char *space;

	memory(long long total_size) {
		space = (char *)malloc(total_size);
	}

	~memory() {
		free(space);
	}
};
struct memory *disk;

long long getblocksize() {
	struct super_block *sb = (struct super_block *)disk->space;
	return sb->block_size;
}

bool my_mount() {

	wd_inode = 0;
	struct inode* wd_ptr = (struct inode *)(disk->space + ((struct super_block *)disk->space)->block_size);
	wd_ptr->file_size = 0;
	wd_ptr->type = TYPE_DIR;

	for(int i=0; i<NUMBER_DP; i++) {
		wd_ptr->dp[i] = -1;
	}
	wd_ptr-> sip = -1;
	wd_ptr-> dip = -1;

	struct open_files ofile;
	ofile.f_inode = wd_inode;
	ofile.read_pointer = 0;
	ofile.write_pointer = 0;
	open_files_table.push_back(ofile);

	return true;
}

void init(unsigned int file_sys_size, unsigned int block_size_kb, char *name) {

	if(block_size_kb * KB_TO_BYTES < MIN_SIZE){
		printf("Blocks are too small\n");
		exit(1);
	}

	unsigned long long numBlocks = file_sys_size/block_size_kb;
	if(numBlocks > 1<<16-1) {
		printf("Too many blocks spoil the broth\n");
		exit(1);
	}

	disk = new memory(KB_TO_BYTES*block_size_kb * numBlocks);

	// define the variables of the super block
	struct super_block *sb = (struct super_block *)(disk->space);
	memcpy(sb->volume_name, name, min(sizeof(sb->volume_name), strlen(name)));
	sb->block_size = block_size_kb * KB_TO_BYTES;
	sb->total_size = file_sys_size * KB_TO_BYTES;
	sb->first_free_block = START_DATA_BLOCK;

	// point each pointer to the next block
	for(int i=START_DATA_BLOCK; i<numBlocks-1; i++) {
		*((int *)(disk->space + i*(sb->block_size))) = i+1;
	}
	*((int *)(disk->space + (numBlocks-1)*(sb->block_size))) = -1;

	if(!my_mount()) {
		printf("Mount failed\n");
		exit(1);
	}
}

int get_pointers(unsigned long long b_size, struct block_pointer &bp) {

	bp.dp = 0;
	bp.sip = 0;
	bp.dip[0] = 0;
	bp.dip[1] = 0;
	
	unsigned long long n_entries = b_size/sizeof(int);
	bp.dp = bp.rp/b_size;
	if(bp.dp >= NUMBER_DP) bp.sip = bp.dp - NUMBER_DP;

	if(bp.sip >= n_entries) {
		bp.rp = bp.rp - (NUMBER_DP + n_entries)*b_size;

		bp.dip[0] = bp.rp/(n_entries * b_size);

		if(bp.dip[0] > n_entries) {
			printf("Insufficient Memory");
			return -1;
		}

		bp.rp = bp.rp%(n_entries*b_size);

		bp.dip[1] = bp.rp/b_size;
		bp.rp = bp.rp%b_size;
	}
	else bp.rp = bp.rp%b_size;

	return 0;
}

int getfreeblock() {
	struct super_block* sb = (struct super_block *)disk->space;

	int first_free_block = sb->first_free_block;

	int next_free_block = *(int *)(disk->space + sb->block_size*first_free_block);

	sb->first_free_block = next_free_block;

	return first_free_block;
}

long long write_file();
long long write_block();

long long read_util();
long long read_block();

long long my_write(int fd, void *buf, long long count) {
	
	if(fd < 0) {
		printf("Invalid file pointer");
		return -1;
	}

	if(count < 0) {
		printf("Invalid number of bits to be read");
		return -1;
	}

	int f_inode = open_files_table[fd].f_inode;
	unsigned long long read_pointer = open_files_table[fd].read_pointer;
	unsigned long long write_pointer = open_files_table[fd].write_pointer;

	char *msg = (char *)buf;
	unsigned long long b_size = ((struct super_block *)disk->space)->block_size;
	struct inode* fd_ptr = (struct inode *)(disk->space + b_size + f_inode * sizeof(struct inode));

	struct block_pointer bp;
	bp.rp = write_pointer;

	get_pointers(b_size, bp);

	char *block_ptr;
	long long bytes_to_write = count, bytes_written = 0;

	fd_ptr->dp[0] = 3;

	for(int i=bp.dp; i<NUMBER_DP; i++) {
		cout<<fd_ptr->dp[i]<<" "<<i<<endl;
		if(fd_ptr->dp[i] == -1 && bytes_to_write>0) {
			int fb = getfreeblock();
			fd_ptr->dp[i] = fb;
			cout<<"fb "<<fb<<endl;
			block_ptr = (char *)(disk->space + fb*b_size);
			if(bytes_to_write >= b_size) {
				memcpy(block_ptr, msg + bytes_written, b_size);
				bytes_written += b_size;
				bytes_to_write -= b_size;
			}
			else {
				memcpy(block_ptr, msg + bytes_written, bytes_to_write);
				bytes_written += bytes_to_write;
				bytes_to_write -= bytes_to_write;
			}
			cout<<"bytes to write "<<bytes_to_write<<endl;
		}
		else {
			cout<<"Writing to old block"<<i<<endl;
			block_ptr = (char *)(disk->space + b_size*fd_ptr->dp[i]);
			int br = (b_size - bp.rp)<bytes_to_write? (b_size - bp.rp) : bytes_to_write;
			memcpy(block_ptr + bp.rp, msg + bytes_written, br);
			bytes_written += br;
			bytes_to_write -= br;
			bp.rp = 0;
		}

		if(bytes_to_write == 0) {
			open_files_table[fd].write_pointer += bytes_written;
			return bytes_written;
		}
	}

	int *addr_ptr;

	if(fd_ptr->sip != -1) {
		addr_ptr = (int *)(disk->space + b_size*fd_ptr->sip);
	}
	else {
		int fb = getfreeblock();
		fd_ptr->sip = fb;
		addr_ptr = (int *)(disk->space + b_size*fd_ptr->sip);

		for(int i=0; i<(b_size/sizeof(int)); i++) {
			*(addr_ptr + i) = -1;
		}
	}

	for(int i = bp.sip; i<(b_size/sizeof(int)); i++) {
		if(*(addr_ptr + i) == -1 && bytes_to_write>0) {
			int fb = getfreeblock();
			*(addr_ptr + i) = fb;
			block_ptr = (char *)(disk->space + fb*b_size);
			if(bytes_to_write >= b_size) {
				memcpy(block_ptr, msg + bytes_written, b_size);
				bytes_written += b_size;
				bytes_to_write -= b_size;
			}
			else {
				memcpy(block_ptr, msg + bytes_written, bytes_to_write);
				bytes_written += bytes_to_write;
				bytes_to_write -= bytes_to_write;
			}			
		}
		else {
			block_ptr = (char *)(disk->space + b_size*fd_ptr->dp[i]);
			int br = (b_size - bp.rp)<bytes_to_write? (b_size - bp.rp) : bytes_to_write;
			memcpy(block_ptr + bp.rp, msg + bytes_written, br);
			bytes_written += br;
			bytes_to_write -= br;
			bp.rp = 0;
		}

		if(bytes_to_write == 0) {
			open_files_table[fd].write_pointer += bytes_written;
			return bytes_written;
		}
	}

	int *dip_ptr;
	if(fd_ptr->dip != -1) {
		dip_ptr = (int *)(disk->space + b_size*fd_ptr->dip);
	}
	else {
		int fb = getfreeblock();
		fd_ptr->dip = fb;
		dip_ptr = (int *)(disk->space + b_size*fd_ptr->dip);
		
		for(int i=0; i<(b_size/sizeof(int)); i++) {
			*(dip_ptr + i) = -1;
		}
	}

	return count;
}

long long my_read(int fd, void *buf, long long count) {
	
	if(fd < 0) {
		printf("Invalid file pointer");
		return -1;
	}

	if(count < 0) {
		printf("Invalid number of bits to be read");
		return -1;
	}

	int f_inode = open_files_table[fd].f_inode;
	unsigned long long read_pointer = open_files_table[fd].read_pointer;
	unsigned long long write_pointer = open_files_table[fd].write_pointer;

	char *msg = (char *)buf;
	unsigned long long b_size = ((struct super_block *)disk->space)->block_size;
	struct inode* fd_ptr = (struct inode *)(disk->space + b_size + f_inode * sizeof(struct inode));

	struct block_pointer bp;
	bp.rp = read_pointer;

	get_pointers(b_size, bp);

	cout<<bp.rp<<" "<<bp.dp<<endl;

	char *block_ptr;
	long long bytes_to_read = count, bytes_read = 0;

	for(int i=bp.dp; i<NUMBER_DP; i++) {
		cout<<fd_ptr->dp[i]<<endl;
		if(fd_ptr->dp[i] != -1 && bytes_to_read>0) {
			block_ptr = (char *)(disk->space + fd_ptr->dp[i]*b_size);
			// cout<<(char *)(block_ptr + bp.rp)<<endl;
			if(bytes_to_read >= (b_size-bp.rp)) {
				memcpy(msg + bytes_read, block_ptr + bp.rp, b_size - bp.rp);
				bytes_read += (b_size - bp.rp);
				bytes_to_read -= (b_size - bp.rp);
				bp.rp = 0;
			}
			else {
				memcpy(msg + bytes_read, block_ptr + bp.rp, bytes_to_read);
				bytes_read += bytes_to_read;
				bytes_to_read -= bytes_to_read;
			}
			cout<<"bytes to write "<<bytes_to_read<<endl;
			cout<<msg<<endl;
		}
		else {
			open_files_table[fd].read_pointer += bytes_read;
			return bytes_read;
		}

		if(bytes_to_read == 0) {
			open_files_table[fd].read_pointer += bytes_read;
			return bytes_read;
		}
	}

	// int *addr_ptr;

	// if(fd_ptr->sip != -1) {
	// 	addr_ptr = (int *)(disk->space + b_size*fd_ptr->sip);
	// }
	// else {
	// 	int fb = getfreeblock();
	// 	fd_ptr->sip = fb;
	// 	addr_ptr = (int *)(disk->space + b_size*fd_ptr->sip);

	// 	for(int i=0; i<(b_size/sizeof(int)); i++) {
	// 		*(addr_ptr + i) = -1;
	// 	}
	// }

	// for(int i = bp.sip; i<(b_size/sizeof(int)); i++) {
	// 	if(*(addr_ptr + i) == -1 && bytes_to_write>0) {
	// 		int fb = getfreeblock();
	// 		*(addr_ptr + i) = fb;
	// 		block_ptr = (char *)(disk->space + fb*b_size);
	// 		if(bytes_to_write >= b_size) {
	// 			memcpy(block_ptr, msg + bytes_written, b_size);
	// 			bytes_written += b_size;
	// 			bytes_to_write -= b_size;
	// 		}
	// 		else {
	// 			memcpy(block_ptr, msg + bytes_written, bytes_to_write);
	// 			bytes_written += bytes_to_write;
	// 			bytes_to_write -= bytes_to_write;
	// 		}			
	// 	}
	// 	else {
	// 		block_ptr = (char *)(disk->space + b_size*fd_ptr->dp[i]);
	// 		int br = (b_size - bp.rp)<bytes_to_write? (b_size - bp.rp) : bytes_to_write;
	// 		memcpy(block_ptr + bp.rp, msg + bytes_written, br);
	// 		bytes_written += br;
	// 		bytes_to_write -= br;
	// 		bp.rp = 0;
	// 	}

	// 	if(bytes_to_write == 0) {
	// 		open_files_table[fd].write_pointer += bytes_written;
	// 		return bytes_written;
	// 	}
	// }

	// int *dip_ptr;
	// if(fd_ptr->dip != -1) {
	// 	dip_ptr = (int *)(disk->space + b_size*fd_ptr->dip);
	// }
	// else {
	// 	int fb = getfreeblock();
	// 	fd_ptr->dip = fb;
	// 	dip_ptr = (int *)(disk->space + b_size*fd_ptr->dip);
		
	// 	for(int i=0; i<(b_size/sizeof(int)); i++) {
	// 		*(dip_ptr + i) = -1;
	// 	}
	// }

	return count;
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

bool parse_path(int &f_inode_n, char *file, long long block_size) {
	struct  inode *f_inode = (struct inode *)(disk->space + block_size + f_inode_n*sizeof(struct inode));
	char *buf = (char *)malloc(f_inode->file_size);

	struct open_files opf;
	opf.f_inode = f_inode_n;
	opf.read_pointer = 0;
	opf.write_pointer = 0;

	open_files_table.push_back(opf);

	my_read(open_files_table.size()-1, buf, 16);

	cout<<"File "<<file<<endl;

	struct directory_data* dd = (struct directory_data *)buf;

	cout<<"dd "<<dd->filename<<endl;

	for(int i=0; i<1; i++) {
		if(!strcmp(dd->filename, file)) {
			cout<<"Success"<<endl;
			f_inode_n = dd->f_inode;
			open_files_table.pop_back();
			return true;
		}
	}
	return false;
}

int myopen(const char *path) {
	char *args[MAX_PATH], arg[MAX_PATH];
	int n = 0;

	strcpy(arg, path);

	n = breakString(args, arg, "/\n");

	for(int i=0; i<n; i++) {
		cout<<args[i]<<endl;
	}

	struct super_block* sb = (struct super_block *)disk->space;
	int curr_inode_n = 0;

	if(path[0]!='/') {
		curr_inode_n = wd_inode;
	}

	cout<<"Arguments :"<<n;
	for(int i=0; i<n; i++) {
		if(!parse_path(curr_inode_n, (char *)args[i], sb->block_size)){
			printf("File not found");
			return -1;
		}
	}
	struct open_files opf;
	opf.f_inode = curr_inode_n;
	opf.read_pointer = 0;
	opf.write_pointer = 0;

	open_files_table.push_back(opf);

	return open_files_table.size()-1;

	return n;
}

void my_mkdir(char *dir) {

	int free_inode = getfreeinode();

	struct directory_data dd;
	strcpy(dd->filename, dir);
	dd.f_inode = free_inode;

	my_write(wd_inode, (char *)&dd, sizeof(dd));

	struct inode* fi = (struct inode *)(disk->space + getblocksize() + free_inode*sizeof(struct inode));
	fi->type = 'd';
	fi->file_size = 0;

	// set all pointers to -1
}

void my_copy(int fd, char *path) {
	// open, read and write
}

void my_copy(char *path, int fd) {
	// open, read and write
}

bool my_rm(int fd) {
	// removes a file
}

bool my_rmdir(char *dir) {
	// to be done in cool head
	int fd;
	struct inode *fd_ptr;

	char *buffer;
	my_read(fd, buffer, fd_ptr->file_size);

	// for each entry recursively call the functions ...
}

int main(int argc, char const *argv[]){
	init(128, 1, (char *)"sda1");

	// open_files_table[0].write_pointer = 1020;
	// open_files_table[0].read_pointer = 1020;
	// getfreeblock();

	// char buf[100] = "Hello Hello Hello ";
	// my_write(0, buf, strlen(buf)-1);
	// my_write(0, buf, strlen(buf)-1);
	// cout<<"Read"<<endl;
	// my_read(0, buf, 40);
	// cout<<"This is the final message" <<buf<<endl;

	struct directory_data dd;
	strcpy(dd.filename, "hello");
	dd.f_inode = 1;

	my_write(0, (char *)&dd, 16);

	int fd = myopen("/hello");

	cout<<"File descriptor"<<fd;

	return 0;
}