#include <iostream>		//for cout
#include <cstdio>		//for perror
#include <fstream>		//for ofstream
#include <sys/types.h>	//for msgget, fork, wait
#include <sys/ipc.h>	//for shmget
#include <sys/shm.h>	//for shmget
#include <sys/msg.h>	//for msgget
#include <cstdlib>		//for exit
#include <string>		//for to_string
#include <cstring>		//for strcpy
#include <unistd.h>		//for fork, pause, usleep
#include <time.h>		//for time
#include <signal.h>		//for signal, SIGUSR1
#include <sys/wait.h>	//for wait

using namespace std;

int isSignalReceived = 0;

//Each entry in page table contains < frame_number, valid/invalid bit >
struct pageTableNode {
	int frameNo;
	int validity;	//0 for invalid, 1 for valid
};

//Process to number of pages mapping
struct procPageMapNode {
	pid_t processId;
	int numPages;
};

void errorCheck(int id, const char *str){
	if(id == -1){
		perror(str);
		exit(-1);
	}
}

void handleSignal(int sig){

	if(sig == SIGUSR1){
		//do nothing, SIGUSR1 which was sent by scheduler in our code has woken up Master
		isSignalReceived = 1;
	}
}

int main(int argc, char* argv[]){

	//Inputs:
	//1) Total number of processes (k)
	//2) Virtual address space – maximum number of pages required per process (m)
	//3) Physical address space – total number of frames in main memory (f)
	//4) Size of the TLB (s)
	//argc should be 4

	if(argc!=5){
		perror("Insufficent number of inputs provided, provide k,m,f and s as command line arguments");
		exit(-1);
	}

	//installing signal handler
	signal(SIGUSR1, handleSignal);

	//initializing random seed
	srand(time(NULL));

	int k,m,f,s;

	k = atoi(argv[1]);
	m = atoi(argv[2]);
	f = atoi(argv[3]);
	s = atoi(argv[4]);

	key_t sm1_key, sm2_key, sm3_key, mq1_key, mq2_key, mq3_key;
	int sm1_id, sm2_id, sm3_id, mq1_id, mq2_id, mq3_id;

	/*
	 The ftok() function uses the identity of the file named by the given pathname (which must refer to an existing, accessible file) and the least significant 8 bits of proj_id (which must be nonzero) to generate a key_t type System V IPC key
	*/

	//creating a file for shared memory
	ofstream fout;
	fout.open("sharedMemory.shm");
	fout.close();
	
	sm1_key = ftok("sharedMemory.shm",50);
	sm2_key = ftok("sharedMemory.shm",80);
	sm3_key = ftok("sharedMemory.shm",110);
	mq1_key = ftok("sharedMemory.shm",140);
	mq2_key = ftok("sharedMemory.shm",170);
	mq3_key = ftok("sharedMemory.shm",200);

	//int shmget(key_t key, size_t size, int shmflg);

	//Page Table is implemented using shared memory (SM1).
	//There will be one page table for each process, where the size of each page table is same as the virtual address space.
	sm1_id = shmget(sm1_key, k*m*sizeof(pageTableNode), IPC_CREAT|0666);
	errorCheck(sm1_id, "Error occurred in getting shared memory for SM1");

	//Free Frame List is implemented using shared memory (SM2).
	//This will contain a list of all free frames in main memory, and will be used by MMU.
	sm2_id = shmget(sm2_key, f*sizeof(int), IPC_CREAT|0666);
	errorCheck(sm2_id, "Error occurred in getting shared memory for SM2");

	//Process to number of pages mapping is to be implemented using shared memory.
	//It contains the number of pages required by every process
	sm3_id = shmget(sm3_key, k*sizeof(procPageMapNode), IPC_CREAT|0666);
	errorCheck(sm3_id, "Error occurred in getting shared memory for SM3");

	//int msgget(key_t key, int msgflg);

	//Ready Queue is used by the scheduler, and is to be implemented using message queue (MQ1).
	mq1_id = msgget(mq1_key, IPC_CREAT|0666);
	errorCheck(mq1_id, "Error occurred in getting message queue identifier for MQ1");

	//MQ2 for communication between Scheduler and MMU.
	mq2_id = msgget(mq2_key, IPC_CREAT|0666);
	errorCheck(mq2_id, "Error occurred in getting message queue identifier for MQ2");

	//MQ3 for communication between Process-es and MMU.
	mq3_id = msgget(mq3_key, IPC_CREAT|0666);
	errorCheck(mq3_id, "Error occurred in getting message queue identifier for MQ3");

	//attaching shared memory to data structures
	pageTableNode* pageTables = (pageTableNode*)shmat(sm1_id,NULL,0);
	int* freeFrameList = (int*)shmat(sm2_id,NULL,0);

	//Initialization of data structures

	//Initially, all frame numbers are equal to -1
	for(int i=0; i<k; i++){
		for(int j=0; j<m; j++){
			pageTables[i*m + j].frameNo = -1;
			pageTables[i*m + j].validity = 0;
		}
	}	

	//all frames are free initially
	for(int i=0; i<f; i++)
		freeFrameList[i] = 1;	//1 means frame is free

	if(shmdt((const void *)pageTables) < 0){
		perror("Error in detaching shared memory for page tables");
		exit(-1);
	}

	if(shmdt((const void *)freeFrameList) < 0){
		perror("Error in detaching shared memory for free frame list");
		exit(-1);
	}

	string sm1Str, sm2Str, sm3Str, mq1Str, mq2Str, mq3Str, KStr, MStr, FStr, SStr;
	sm1Str = to_string(sm1_id);
	sm2Str = to_string(sm2_id);
	sm3Str = to_string(sm3_id);
	mq1Str = to_string(mq1_id);
	mq2Str = to_string(mq2_id);
	mq3Str = to_string(mq3_id);
	KStr = to_string(k);
	MStr = to_string(m);
	FStr = to_string(f);
	SStr = to_string(s);

	pid_t scheduler = fork();

	if(scheduler == 0){
		// 	Creates the Scheduler module
		// 	A separate Linux process for scheduling the Process-es from ready queue (MQ1). It passes the parameters (MQ1, MQ2, k) via command-line arguments during process creation.
		char** args = new char*[5];

		args[0] = new char[15];					//for ./sched
		args[1] = new char[mq1Str.size()+1];	//for mq1Str
		args[2] = new char[mq2Str.size()+1];	//for mq2Str
		args[3] = new char[KStr.size()+1];		//for KStr
		args[4] = NULL;

		strcpy(args[0],"./sched");
		strcpy(args[1],mq1Str.c_str());
		strcpy(args[2],mq2Str.c_str());
		strcpy(args[3],KStr.c_str());

		//calling scheduler
		if(execvp(args[0], args)<0){
			perror("Error in execvp in creating scheduler");
			exit(-1);
		}				
	}

	pid_t mmu = fork();

	if(mmu == 0){
		// 	Creates the MMU module
		// 	A separate Linux process for translating page numbers to frame numbers and to handle page faults. It passes the parameters (MQ2, MQ3, SM1, SM2, SM3, k, m, f, s) via command line arguments during process creation.
		char** args = new char*[11];

		args[0] = new char[10];					//for ./MMU
		args[1] = new char[mq2Str.size()+1];	//for mq2Str
		args[2] = new char[mq3Str.size()+1];	//for mq3Str
		args[3] = new char[sm1Str.size()+1];	//for sm1Str
		args[4] = new char[sm2Str.size()+1];	//for sm2Str
		args[5] = new char[sm3Str.size()+1];	//for sm3Str
		args[6] = new char[KStr.size()+1];		//for KStr
		args[7] = new char[MStr.size()+1];		//for MStr
		args[8] = new char[FStr.size()+1];		//for FStr
		args[9] = new char[SStr.size()+1];		//for SStr
		args[10] = NULL;

		strcpy(args[0],"./MMU");
		strcpy(args[1],mq2Str.c_str());
		strcpy(args[2],mq3Str.c_str());
		strcpy(args[3],sm1Str.c_str());
		strcpy(args[4],sm2Str.c_str());
		strcpy(args[5],sm3Str.c_str());
		strcpy(args[6],KStr.c_str());
		strcpy(args[7],MStr.c_str());
		strcpy(args[8],FStr.c_str());
		strcpy(args[9],SStr.c_str());

		//calling mmu
		if(execvp(args[0], args)<0){
			perror("Error in execvp in creating mmu");
			exit(-1);
		}
	}

	pid_t Process_es[k];

	//attaching memory for process number of pages mapping
	procPageMapNode* processPageMap = (procPageMapNode*)shmat(sm3_id,NULL,0);

	//initializing process page map
	for(int i=0; i<k; i++){
		processPageMap[i].processId = -1;
		processPageMap[i].numPages = -1;
	}

	for(int i=0; i<k ; i++){
		//Creates k number of Process-es as separate Linux processes at fixed interval of time (250 ms).

		//Selects a random number between (1,m)
		//Assigns it as the required number of pages for a particular process
		int mi = rand()%m + 1;
		processPageMap[i].numPages = mi;

		//Generates the page reference string (R i) for every process (P i)
		//Length of reference string for Pi = Randomly select between 2*mi and 10*mi.
		int li = rand()%(8*mi + 1) + 2*mi;
		string pageRefString = "";

		for(int j=0; j<li; j++){
			pageRefString += to_string(rand()%mi);

			if(j!=li-1)
				pageRefString += ",";
		}

		Process_es[i] = fork();

		if(Process_es[i] == 0){

			processPageMap[i].processId = getpid();

			//Passes page reference string, MQ1, MQ3 to every process
			char *args[5];

			args[0] = new char[10];							//for ./proc
			args[1] = new char[pageRefString.size()+1];		//for page reference string
			args[2] = new char[mq1Str.size()+1];			//for mq1Str
			args[3] = new char[mq3Str.size()+1];			//for mq3Str
			args[4] = NULL;

			strcpy(args[0],"./process");
			strcpy(args[1],pageRefString.c_str());
			strcpy(args[2],mq1Str.c_str());
			strcpy(args[3],mq3Str.c_str());

			//calling process
			if(execvp(args[0], args)<0){
				perror("Error in execvp in creating process");
				exit(-1);
			}
		}

		usleep(250000);
	}

	if(shmdt((const void *)processPageMap) < 0){
		perror("Error in detaching shared memory for processPageMap");
		exit(-1);
	}

	//pausing master if signal is not received yet
	if(isSignalReceived == 0)
		pause();

	//Receives notification from scheduler about termination: Terminates Scheduler, MMU and then terminates itself.
	kill(scheduler, SIGKILL);
	kill(mmu, SIGKILL);

	//wait for all processes so that they do not remain zombie processes for long
	for(int i=0; i<k ; i++){
		wait(NULL);
	}
	
	//to destroy the shared memory
	shmctl(sm1_id,IPC_RMID,NULL);
	shmctl(sm2_id,IPC_RMID,NULL);
	shmctl(sm3_id,IPC_RMID,NULL);

	//to destroy the message queue 
    msgctl(mq1_id, IPC_RMID, NULL);
    msgctl(mq2_id, IPC_RMID, NULL);
	msgctl(mq3_id, IPC_RMID, NULL);

	remove("sharedMemory.shm");

	return 0;
}