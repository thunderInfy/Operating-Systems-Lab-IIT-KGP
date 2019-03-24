#include <iostream>
#include <cstdio>		//for perror
#include <cstdlib>		//for exit
#include <sys/types.h>	//for shmat, msgrcv
#include <sys/shm.h>	//for shmat
#include <sys/ipc.h>	//for msgrcv
#include <sys/msg.h>	//for msgrcv
#include <map>			//for map

using namespace std;

//initializing timestamp is 0
int timestamp = 0;

struct msgPageFrameBuffer{
	long mesg_type;
	pid_t pid;				//process id
	int data;				//data can be page or frame number
};

struct schedMsgContainer{
	long mesg_type;
	string mesg;
};

//Each entry in page table contains < frame_number, valid/invalid bit >
struct pageTableNode {
	int frameNo;
	int validity;			//0 for invalid, 1 for valid
};

//Process to number of pages mapping
struct procPageMapNode {
	pid_t processId;
	int numPages;
};

//structure to hold process id and page no.
struct virtualInfo {
	pid_t pid;
	int pageNum;

	virtualInfo(){
		this->pid = -1;
		this->pageNum = -1;
	}

    bool operator<(const virtualInfo& t) const
    { 
        return (this->pid < t.pid); 
    } 
};

//structure to hold frameNo and timestamp
struct physicalInfo {
	int frameNo;
	int timestamp;

	physicalInfo(){
		this->frameNo = -1;
		this->timestamp = -1;
	}
};

//creating tlb
map < virtualInfo, physicalInfo > tlb;

int main(int argc, char* argv[]){

	//parameters (MQ2, MQ3, SM1, SM2, SM3, k, m , f, s)
	//s is size of TLB
	//Translates the page number to frame number and handles page-fault, and also manages the page table

	if(argc!=10){
		perror("Insufficent number of inputs provided, provide MQ2, MQ3, SM1, SM2, SM3, k, m, f and s as command line arguments");
		exit(-1);
	}

	int mq2_id, mq3_id, sm1_id, sm2_id, sm3_id, k, m, f, s;

	mq2_id = atoi(argv[1]);
	mq3_id = atoi(argv[2]);
	sm1_id = atoi(argv[3]);
	sm2_id = atoi(argv[4]);
	sm3_id = atoi(argv[5]);
	k = atoi(argv[6]);
	m = atoi(argv[7]);
	f = atoi(argv[8]);	
	s = atoi(argv[9]);

	//attaching shared memory to data structures
	pageTableNode* pageTables = (pageTableNode*)shmat(sm1_id,NULL,0);
	int* freeFrameList = (int*)shmat(sm2_id,NULL,0);
	procPageMapNode* processPageMap = (procPageMapNode*)shmat(sm3_id,NULL,0);
 
	msgPageFrameBuffer msgPFB;

	int counter = 0;

	while(counter < k){
		//MMU wakes up after receiving the page number via message queue (MQ3) from Process.
		msgrcv(mq3_id, &msgPFB, sizeof(msgPFB), 1, 0);

		if(msgPFB.data == -9){
		// 	If MMU receives the page number -9 via message queue: 
		// 		Infers that the process has completed its execution
		// 		Updates the free-frame list
		
		// 		Sends Type II message to the Scheduler for scheduling the next process

		}

		// 	It receives the page number from the process and checks the TLB, failing which, it checks the page table for the corresponding process. There can be following two cases:
		// 		If the page is already in page table, MMU sends back the corresponding frame number to the process.
		// 		Else in case of page fault
		// 			Sends -1 to the process as frame number
		// 			Invokes the page fault handler to handle the page fault
		// 				If free frame available
		// 					updates the page table and also updates the corresponding free-frame list.
		// 				Else 
		// 					Does local page replacement. Select victim page using LRU, replace it and brings in a new frame and update the page table.
		// 			Intimates the Scheduler by sending Type I message to enqueue the current process and schedule the next process.
		// Maintains a global timestamp (count), which is incremented by 1 after every page reference.
	}

	return 0;
}