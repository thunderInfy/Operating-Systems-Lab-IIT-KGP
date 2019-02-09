#include <iostream>
#include <cstdio>
#include <pthread.h>
#include <cstdlib>
#include <ctime>
#include <signal.h>
#include <unistd.h>

using namespace std;

void* doJobs(void*);

int* status;

//initially all threads will do nothing
int init = 0;

class myThreads{

	public:

		pthread_t 	tid;
		char 		type;
		int 		threadNum;

	private:
		void initType(){
			//initializing type of thread with 'P' or 'C' each having equal probability
			if((rand() % 2)==0){
				this->type = 'P';
			}
			else{
				this->type = 'C';
			}
		}

	public:

		void createMe(int index){
			this->initType();
			this->threadNum = index;
			pthread_create(&this->tid, NULL, doJobs, (void *) this);
		}

		void join(){
			pthread_join(this->tid, NULL);
		}

};

void sleepOrWake(int signo){

	if(signo==SIGUSR1){
		cout<<"I am sleeping\n";
	}
	else if(signo==SIGUSR2){
		cout<<"Thanks for waking me up\n";
	}
}

void* doJobs(void *param){
	
	//don't start the execution unless the scheduler thread sets init to 1
	while(init==0);

	myThreads *t;
	t = (myThreads *) param;
	
	//installing signal handlers here
	signal(SIGUSR1, sleepOrWake);
	signal(SIGUSR2,	sleepOrWake);


	if(t->type=='P'){
		raise(SIGUSR1);
	}
	else{
		raise(SIGUSR2);
	}

}

void* scheduler(void *param){
	init = 1;
}

int main() {
	
	//specifying random seed
	srand((unsigned)time(NULL));

	//Specifying value of N
	int N = 10;

	//dynamically create status array
	status = new int[N];

	//initializing status array to all zeros
	for(int i=0;i<N;i++){
		status[i] = 0;
	}

	pthread_t 	SchId;

	//create scheduler thread
	pthread_create(&SchId, NULL, scheduler, NULL);

	//threadObj will be a dynamic array of thread objects
	myThreads *threadObj;
	threadObj = new myThreads[N];

	//create threads
	for(int i=0;i<N;i++){
		threadObj[i].createMe(i);
	}

	//join threads
	for(int i=0;i<N;i++){
		threadObj[i].join();
	}

	//delete the dynamic array of threadObj
	delete threadObj;

	//delete the dynamic array of status
	delete status;

	return 0;
}

