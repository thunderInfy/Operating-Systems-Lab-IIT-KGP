#include <iostream>		
#include <cstdio>	
#include <cstdlib>			//for exit
#include <sys/types.h>		//for getpid 
#include <unistd.h>			//for getpid
#include <sys/ipc.h>		//for msgsnd
#include <sys/msg.h>		//for msgsnd
#include <vector>			//for vector
#include <sstream>			//for stringstream
#include <string>			//for string
#include <signal.h>			//for signal, SIGUSR1

using namespace std;

struct messageBuffer{
	long mesg_type;
	pid_t pid;
};

struct msgPageFrameBuffer{
	long mesg_type;
	pid_t pid;				//process id
	int data;				//data can be page or frame number
};

int isSignalReceived = 0;

void handleSignal(int sig){
	if(sig == SIGUSR1){
		//do nothing, SIGUSR1 which was sent by scheduler in our code has woken up process
		isSignalReceived = 1;
	}

	//installing signal handler
	signal(SIGUSR1, handleSignal);
}

int main(int argc, char* argv[]){

	//parameters (page reference string, MQ1, MQ3)

	if(argc!=4){
		perror("Insufficent number of inputs provided, provide page reference string, MQ1 and MQ3 as command line arguments");
		exit(-1);
	}

	//installing signal handler
	signal(SIGUSR1, handleSignal);

	int mq1_id, mq3_id;
	pid_t myID = getpid();

	//argv[1] contains page reference string
	
	string pageRefStr(argv[1]);			//pageRefStr contains C++ style string corresponding to argv[1]
	mq1_id = atoi(argv[2]);				//ready queue
	mq3_id = atoi(argv[3]);

	vector <int> pageNumVec;			//vector to store page numbers from page reference string
	stringstream buffer(pageRefStr);	//buffer is the string stream here
	string temp;

	while(getline(buffer, temp, ',')){
		pageNumVec.push_back(atoi(temp.c_str()));
	}

	messageBuffer mesg;
	mesg.mesg_type = 1;
	mesg.pid = myID;

	//adding current process to ready queue
    msgsnd(mq1_id, &mesg, sizeof(mesg), 0);

    //pausing process if signal is not yet received
    if(isSignalReceived == 0)
 	   pause();

 	msgPageFrameBuffer msgPFB;

 	for(int i=0; i<pageNumVec.size(); ){

	 	//Execution of process means generation of page numbers from reference string 

 		msgPFB.mesg_type = 1;
 		msgPFB.pid = myID;
		msgPFB.data = pageNumVec[i];

	 	//Sends the page number to MMU using message queue (MQ3)
	 	msgsnd(mq3_id, &msgPFB, sizeof(msgPFB), 0);

	 	//Receives the frame number from MMU with message type myID = getpid()
		msgrcv(mq3_id, &msgPFB, sizeof(msgPFB), myID, 0);

		if(msgPFB.data >= 0){
			// 	If MMU sends a valid frame number:
			// 		It parses the next page number in the reference string and goes in loop.
			i++;
		}
		else if(msgPFB.data == -1){
			// 	Else, in case of page fault:
			// 		Gets -1 as frame number from MMU.
			// 		Saves the current element of the reference string for continuing its execution when it is scheduled next. 
			// 		Goes into wait(). MMU invokes page fault handling routine to handle the page fault. 
			// 			Note : The current process is out of Ready queue and Scheduler enqueues it to the Ready queue once page fault is resolved.
			pause();
		}
		else if(msgPFB.data == -2){
			// 	Else, in case of invalid page reference:
			// 		It gets -2 as frame number from MMU and terminates itself. 
			return 0;
		}
		else{	
			perror("Invalid integer returned!, should be either >=0 or -1 or -2\n");
			exit(-1);
		}
 	}

 	// When the Process completes the scanning of the reference string, it sends -9 (marker to denote end of page reference string) to the MMU.
 	msgPFB.mesg_type = 1;
 	msgPFB.pid = myID;
	msgPFB.data = -9;

	//Sends the page number to MMU using message queue (MQ3)
	msgsnd(mq3_id, &msgPFB, sizeof(msgPFB), 0);
	
	return 0;
}