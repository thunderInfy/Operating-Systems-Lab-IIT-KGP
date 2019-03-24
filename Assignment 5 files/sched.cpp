#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <signal.h>		//for signal, SIGUSR1
#include <sys/types.h>	//for getppid
#include <unistd.h>		//for getppid
#include <sys/ipc.h>	//for msgrcv
#include <sys/msg.h>	//for msgrcv

using namespace std;

struct messageBuffer{
	long mesg_type;
	pid_t pid;
};

int main(int argc, char* argv[]){

	//parameters (MQ1, MQ2)

	if(argc!=3){
		perror("Insufficent number of inputs provided, provide MQ1 and MQ2 as command line arguments");
		exit(-1);
	}

	int mq1_id, mq2_id;

	mq1_id = atoi(argv[1]);			//MQ1 is ready queue
	mq2_id = atoi(argv[2]);

	messageBuffer mesg;

	//Initially when the ready queue is empty, it waits on the ready queue (MQ1).
	msgrcv(mq1_id,&mesg,sizeof(mesg),1,0);

	//Notifies Master after all processes have finished execution
	pid_t masterPID = getppid();
	kill(masterPID, SIGUSR1);
	
	// Schedules various processes using FCFS algorithm.
	// 		Continuously scans the ready queue and selects processes in FCFS order for scheduling. 
	// 		Once a process gets added, it starts scheduling: 
	// 			It selects the first process from ready queue.
	// 			Removes it from the queue. 
	// 			Sends it a signal for starting execution. 
	// 			Then the scheduler blocks itself till gets notification message from MMU. It can receive two types of message from MMU:
	// 				Type I message “PAGE FAULT HANDLED”:
	// 					After successful page-fault handling.
	// 					After getting this signal:
	// 						It enqueues the current process and schedules the first process from the Ready queue.
	// 					Type II message “TERMINATED”:
 	// 						After successful termination of process.
	// 					After getting this signal: 
	// 						It schedules the first process from Ready queue.


	return 0;
}