#include <iostream>
#include <cstdio>		//for perror
#include <cstdlib>		//for exit

using namespace std;

//struct 

int main(int argc, char* argv[]){

	//parameters (MQ2, MQ3, SM1, SM2)
	//Translates the page number to frame number and handles page-fault, and also manages the page table

	if(argc!=5){
		perror("Insufficent number of inputs provided, provide MQ2, MQ3, SM1 and SM2 as command line arguments");
		exit(-1);
	}

	int mq2_id, mq3_id, sm1_id, sm2_id;

	mq2_id = atoi(argv[1]);
	mq3_id = atoi(argv[2]);
	sm1_id = atoi(argv[3]);
	sm2_id = atoi(argv[4]);

	// 	MMU wakes up after receiving the page number via message queue (MQ3) from Process.
	//msgrcv(mq3_id);


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
	// 	If MMU receives the page number -9 via message queue: 
	// 		Infers that the process has completed its execution.
	// 		Updates the free-frame list.
	// 		Releases all allocated frames.
	// 		Sends Type II message to the Scheduler for scheduling the next process.
	// Maintains a global timestamp (count), which is incremented by 1 after every page reference.

	return 0;
}