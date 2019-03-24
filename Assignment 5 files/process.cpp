#include <iostream>		
#include <cstdio>	
#include <cstdlib>			//for exit
#include <sys/types.h>		//for getpid 
#include <unistd.h>			//for getpid
#include <sys/ipc.h>		//for msgsnd
#include <sys/msg.h>		//for msgsnd


using namespace std;

struct messageBuffer{
	long mesg_type;
	pid_t pid;
};


int main(int argc, char* argv[]){

	//parameters (page reference string, MQ1, MQ3)

	if(argc!=4){
		perror("Insufficent number of inputs provided, provide page reference string, MQ1 and MQ3 as command line arguments");
		exit(-1);
	}

	int mq1_id, mq3_id;

	//argv[1] contains page reference string
	
	mq1_id = atoi(argv[2]);			//ready queue
	mq3_id = atoi(argv[3]);

	messageBuffer mesg;
	mesg.mesg_type = 1;
	mesg.pid = getpid();

	//adding current process to ready queue
    //msgsnd(mq1_id, &mesg, sizeof(mesg), 0);

	/*
	Process: 
		Execution of process means generation of page numbers from reference string. 
		Sends the page number to MMU using message queue (MQ3) and receives the frame number from MMU.
			If MMU sends a valid frame number:
				It parses the next page number in the reference string and goes in loop.
			Else, in case of page fault:
				Gets -1 as frame number from MMU.
				Saves the current element of the reference string for continuing its execution when it is scheduled next. 
				Goes into wait(). MMU invokes page fault handling routine to handle the page fault. 
					Note : The current process is out of Ready queue and Scheduler enqueues it to the Ready queue once page fault is resolved.
			Else, in case of invalid page reference:
				It gets -2 as frame number from MMU and terminates itself. 
				The MMU informs the scheduler to schedule the next process.
		When the Process completes the scanning of the reference string, it sends -9 (marker to denote end of page reference string) to the MMU.
		MMU will notify the Scheduler.
	*/

	return 0;
}