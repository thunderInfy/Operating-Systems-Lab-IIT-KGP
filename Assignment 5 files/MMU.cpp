#include <iostream>
#include <cstdio>		//for perror
#include <cstdlib>		//for exit
#include <sys/types.h>	//for shmat, msgrcv
#include <sys/shm.h>	//for shmat
#include <sys/ipc.h>	//for msgrcv
#include <sys/msg.h>	//for msgrcv
#include <map>			//for map
#include <iterator>		//for iterator
#include <cstring>		//for strcpy
#include <unistd.h>		//for pause

using namespace std;

//initializing timestamp is 0
int timestamp = 0;
int mq2_id, mq3_id, sm1_id, sm2_id, sm3_id, k, m, f, s;
int** pageTimeStamps;

struct msgPageFrameBuffer{
	long mesg_type;
	pid_t pid;				//process id
	int data;				//data can be page or frame number
};

struct schedMsgContainer{
	long mesg_type;
	char mesg[30];
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
    	if(this->pid != t.pid){
    		return (this->pid < t.pid);
    	}
    	else{
    		return (this->pageNum < t.pageNum);
    	} 
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

//creating processToIndex
map < pid_t, int > processToIndex;

pageTableNode* pageTables;
int* freeFrameList;
procPageMapNode* processPageMap;

void updateMap(pid_t processidentity){

	int temp;

	auto itr = processToIndex.find(processidentity);

	if(itr==processToIndex.end()){
		temp = processToIndex.size();
		processToIndex[processidentity] = temp;
	} 
}

void updateFreeFrameList(pid_t processidentity){
	
	int index  = processToIndex[processidentity];

	for(int j = 0; j < m; j++){
		if(pageTables[index*m + j].validity == 1){
			freeFrameList[pageTables[index*m + j].frameNo] = 1;
			pageTables[index*m + j].validity = 0;
		}	
	}
}

int checkValidity(int pageNo, pid_t processidentity){

	for(int i = 0; i<k ; i++){
		if(processPageMap[i].processId == processidentity){

			if(pageNo < processPageMap[i].numPages && pageNo >= 0){
				return 0;
			}
			break;
		}
	}
	return -1;
}

void replyToProcess(msgPageFrameBuffer msgPFB, int frameNo){
	msgPFB.mesg_type = msgPFB.pid;
	msgPFB.data = frameNo;

	msgsnd(mq3_id, &msgPFB, sizeof(msgPFB), 0);
}

int checkTLB(msgPageFrameBuffer msgPFB){

	virtualInfo v;
	v.pid = msgPFB.pid;
	v.pageNum = msgPFB.data;

	auto itr = tlb.find(v);

	if(itr==tlb.end()){
		return -1;
	}

	//found in tlb, sending frame number to process

	tlb[v].timestamp = timestamp;
	pageTimeStamps[processToIndex[v.pid]][v.pageNum] = timestamp;

	replyToProcess(msgPFB, tlb[v].frameNo);
	
	return 0;
}

void updateTLB(int pageNo, pid_t pid, int frameNum){

	virtualInfo v;
	physicalInfo p;

	v.pid = pid;
	v.pageNum = pageNo;

	p.frameNo = frameNum;
	p.timestamp = timestamp;

	if(tlb.size() >= s){

		int difference = -1;
		int maxDifference = -1;

		map < virtualInfo, physicalInfo >::iterator itr, maxItr;

		for(itr = tlb.begin(); itr!=tlb.end(); itr++){

			difference = timestamp - itr->second.timestamp;

			if(difference > maxDifference){
				maxDifference = difference;
				maxItr = itr;
			}

		}
		tlb.erase(itr->first);
	}

	tlb[v] = p;
}

int checkPageTable(msgPageFrameBuffer &msgPFB){

	int index = processToIndex[msgPFB.pid];
	int pageNum = msgPFB.data;

	if(pageTables[index*m+pageNum].validity == 0){
		return -1;
	}

	//the page is already in page table, MMU sends back the corresponding frame number to the process

	updateTLB(pageNum, msgPFB.pid, pageTables[index*m+pageNum].frameNo);

	pageTimeStamps[index][pageNum] = timestamp;

	replyToProcess(msgPFB, pageTables[index*m+pageNum].frameNo);

	return 0;
}

int getFreeFrame(){
	for(int i=0; i<f; i++){
		if(freeFrameList[i] == 1){
			return i;
		}
	}
	return -1;
}

void HandlePageFault(int pageNum, pid_t pid){

	int index = processToIndex[pid];
	int fr = getFreeFrame();

	if(fr < 0){
		//Does local page replacement. Select victim page using LRU, replace it and brings in a new frame and update the page table.

		int difference;
		int maxDifference = -1, jVal = -1;

		for(int j=0; j<m; j++){
			if(pageTables[index*m+j].validity == 1){

				difference = timestamp - pageTimeStamps[index][j];

				if(maxDifference < difference){
					maxDifference = difference;
					jVal = j;
				}
			}
		}

		if(maxDifference != -1){
			//Victim Page found
			pageTables[index*m + pageNum].frameNo = pageTables[index*m + jVal].frameNo;
			pageTables[index*m + jVal].validity = 0;
			pageTables[index*m + pageNum].validity = 1;

			pageTimeStamps[index][pageNum] = timestamp;

			updateTLB(pageNum, pid, pageTables[index*m+pageNum].frameNo);
		}
		/*else{
			//Local Page Replacement failed
			//Doing global page replacement

			maxDifference = -1;
			jVal = -1;
			int iVal = -1;

			for(int i=0; i<k; i++){
				for(int j=0; j<m; j++){
					if(pageTables[i*m+j].validity == 1){

						difference = timestamp - pageTimeStamps[i][j];

						if(maxDifference < difference){
							maxDifference = difference;
							jVal = j;
							iVal = i;
						}
					}
				}
			}

			if(maxDifference == -1 || jVal == -1 || iVal == -1){
				perror("Both Local and Global Page Replacement Algorithms failed!");
				exit(-1);
			}

			pageTables[index*m + pageNum].frameNo = pageTables[iVal*m + jVal].frameNo;
			pageTables[iVal*m + jVal].validity = 0;
			pageTables[index*m + pageNum].validity = 1;

			pageTimeStamps[index][pageNum] = timestamp;

		}*/

	}
	else{
		//free frame available
		//updates the page table
		pageTables[index*m+pageNum].frameNo = fr;
		pageTables[index*m+pageNum].validity = 1;

		//updates the corresponding free-frame list
		freeFrameList[fr] = 0;

		pageTimeStamps[index][pageNum] = timestamp;
	
		updateTLB(pageNum, pid, fr);
	}

}


int main(int argc, char* argv[]){

	//parameters (MQ2, MQ3, SM1, SM2, SM3, k, m , f, s)
	//s is size of TLB
	//Translates the page number to frame number and handles page-fault, and also manages the page table

	if(argc!=10){
		perror("Insufficent number of inputs provided, provide MQ2, MQ3, SM1, SM2, SM3, k, m, f and s as command line arguments");
		exit(-1);
	}

	mq2_id = atoi(argv[1]);
	mq3_id = atoi(argv[2]);
	sm1_id = atoi(argv[3]);
	sm2_id = atoi(argv[4]);
	sm3_id = atoi(argv[5]);
	k = atoi(argv[6]);
	m = atoi(argv[7]);
	f = atoi(argv[8]);	
	s = atoi(argv[9]);

	pageTimeStamps = new int*[k];
	for(int i=0; i<k ;i++){
		pageTimeStamps[i] = new int[m];
	}

	for(int i=0; i<k; i++){
		for(int j=0; j<m; j++){
			pageTimeStamps[i][j] = -1;
		}
	}

	//attaching shared memory to data structures
	pageTables = (pageTableNode*)shmat(sm1_id,NULL,0);
	freeFrameList = (int*)shmat(sm2_id,NULL,0);
	processPageMap = (procPageMapNode*)shmat(sm3_id,NULL,0);
 
	msgPageFrameBuffer msgPFB;
	schedMsgContainer schMsgC;

	int counter = 0;

	while(counter < k){
		//MMU wakes up after receiving the page number via message queue (MQ3) from Process.
		msgrcv(mq3_id, &msgPFB, sizeof(msgPFB), 1, 0);

		cout<<'('<<timestamp<<','<<msgPFB.pid<<','<<msgPFB.data<<")\n";
		fflush(stdout);

		updateMap(msgPFB.pid);

		if(msgPFB.data == -9){
			//If MMU receives the page number -9 via message queue, it infers that the process has completed its execution
			//Updates the free-frame list
			updateFreeFrameList(msgPFB.pid);

			schMsgC.mesg_type = 1;
			strcpy(schMsgC.mesg,"TERMINATED");

			//Sends Type II message to the Scheduler for scheduling the next process
			msgsnd(mq2_id, &schMsgC, sizeof(schMsgC), 0);

			//increment counter
			counter++;
		}
		else{
			//check if msgPFB.data is a valid page number
			if(checkValidity(msgPFB.data, msgPFB.pid) < 0){

				replyToProcess(msgPFB, -2);

				updateFreeFrameList(msgPFB.pid);

				schMsgC.mesg_type = 1;
				strcpy(schMsgC.mesg,"TERMINATED");

				//Sends Type II message to the Scheduler for scheduling the next process
				msgsnd(mq2_id, &schMsgC, sizeof(schMsgC), 0);

				//increment counter
				counter++;
			}
			else{
				
				if(checkTLB(msgPFB) < 0){

					//not found in tlb
					//it checks the page table for the corresponding process. There can be following two cases:
					
					if(checkPageTable(msgPFB) < 0 ){

						//Page fault
						//Sends -1 to the process as frame number
						replyToProcess(msgPFB, -1);

						cout<<'('<<msgPFB.pid<<','<<msgPFB.data<<")\n";
						fflush(stdout);

						//Invokes the page fault handler to handle the page fault
						HandlePageFault(msgPFB.data, msgPFB.pid);						

						// 	Intimates the Scheduler by sending Type I message to enqueue the current process and schedule the next process.

						schMsgC.mesg_type = 1;
						strcpy(schMsgC.mesg,"PAGE FAULT HANDLED");

						msgsnd(mq2_id, &schMsgC, sizeof(schMsgC), 0);
					}
				}

			}	
		}	

		// Maintains a global timestamp (count), which is incremented by 1 after every page reference.
		timestamp++;
	}

	if(shmdt((const void *)pageTables) < 0){
		perror("Error in detaching shared memory for page tables");
		exit(-1);
	}

	if(shmdt((const void *)freeFrameList) < 0){
		perror("Error in detaching shared memory for free frame list");
		exit(-1);
	}
	if(shmdt((const void *)processPageMap) < 0){
		perror("Error in detaching shared memory for processPageMap");
		exit(-1);
	}

	for(int i=0; i<k ;i++){
		delete pageTimeStamps[i];
	}
	delete pageTimeStamps;

	pause();

	return 0;
}