#ifndef MYFS_HEADER
#define MYFS_HEADER

#include <cstring>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <vector>
#include <climits>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
using namespace std;

#define MIN_BLOCK_SIZE 32				
#define KB_TO_BYTES 1024
#define START_DATA_BLOCK 3
#define NUMBER_OF_DP 5
#define MAX_PATH 100
#define TYPE_DIR false
#define TYPE_FILE true
#define VALID true
#define INVALID false

//API function prototypes
void init(unsigned int file_sys_size, unsigned int block_size_kb, char *name);
int my_open(const char *filename);
int my_close(int fd);
unsigned long long my_read(int fd, char *buf, unsigned long long count);
unsigned long long my_write(int fd, char *buf, unsigned long long count);
bool my_copy(int fd, const char *path);
bool my_copy(const char *path, int fd);
unsigned long long my_cat(int fd);
int my_mkdir(const char *dirname);
int my_rmdir(const char *dirname, int recurseCount=0);
int my_chdir(const char *dirname);

#endif