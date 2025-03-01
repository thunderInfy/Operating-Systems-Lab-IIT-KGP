#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <signal.h>		//for signal, SIGUSR1
#include <sys/types.h>	//for getppid
#include <unistd.h>		//for getppid
#include <sys/ipc.h>	//for msgrcv
#include <sys/msg.h>	//for msgrcv
#include <cstring>		//for strcpy

using namespace std;

struct messageBuffer{
	long mesg_type;
	pid_t pid;
};

struct schedMsgContainer{
	long mesg_type;
	char mesg[30];
};

int main(int argc, char* argv[]){

	//parameters (MQ1, MQ2, k)

	if(argc!=4){
		perror("Insufficent number of inputs provided, provide MQ1, MQ2 and k as command line arguments");
		exit(-1);
	}

	int mq1_id, mq2_id, k;

	mq1_id = atoi(argv[1]);			//MQ1 is ready queue
	mq2_id = atoi(argv[2]);
	k = atoi(argv[3]);				//k is number of processes

	int counter = 0;

	while(counter < k){

		messageBuffer mesg;
		schedMsgContainer schMsgC;

		//Schedules various processes using FCFS algorithm.
		//Continuously scans the ready queue and selects processes in FCFS order for scheduling		

		//Initially when the ready queue is empty, it waits on the ready queue (MQ1).
		//Once a process gets added, it starts scheduling. 
		//It selects the first process from ready queue.
		//Removes it from the queue
		msgrcv(mq1_id,&mesg,sizeof(mesg),1,0);			//msgrcv is blocking

		//Sends it a signal for starting execution
		kill(mesg.pid, SIGUSR1);

		//Then the scheduler blocks itself till it gets notification message from MMU. It can receive two types of message from MMU
		msgrcv(mq2_id,&schMsgC,sizeof(schMsgC),1,0);

		if(strcmp(schMsgC.mesg,"PAGE FAULT HANDLED") == 0){

			//Type I message “PAGE FAULT HANDLED”:
			//	After successful page-fault handling.
			// 	After getting this signal:
			// 		It enqueues the current process and schedules the first process from the Ready queue.
			mesg.mesg_type = 1;
			msgsnd(mq1_id,&mesg,sizeof(mesg),0);
		}
		else{
			//Type II message “TERMINATED”:
 			//	After successful termination of process.
			//	After getting this signal: 
			//		It schedules the first process from Ready queue.
			counter++;
		}
	}

	//Notifies Master after all processes have finished execution
	pid_t masterPID = getppid();
	kill(masterPID, SIGUSR1);

	pause();
	return 0;
}