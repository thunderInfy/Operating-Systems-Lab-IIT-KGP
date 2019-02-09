#include <iostream>
#include <cstdio>
#include <pthread.h>
#include <cstdlib>
#include <ctime>
#include <signal.h>
#include <unistd.h>
#include <map>

using namespace std;

void* doJobs(void*);
void sleepOrWake(int);

int* status;

class myThreads{

	public:

		pthread_t 	tid;
		char 		type;
		int 		threadNum;
		int 		state;	//state = 0 means sleep, 1 means awake and 2 means complete

		// a map to get threadNum from tid
		static map <pthread_t, int> m;	

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
			this->state = 0;
			pthread_create(&this->tid, NULL, doJobs, (void *) this);
		}

		void join(){
			pthread_join(this->tid, NULL);
		}

};

//threadObj will be a dynamic array of thread objects
myThreads *threadObj;
map <pthread_t, int> myThreads::m;

void installSignalHandler(){
	//installing signal handlers here
	signal(SIGUSR1, sleepOrWake);
	signal(SIGUSR2,	sleepOrWake);
}

void sleepOrWake(int signo){

	if(signo==SIGUSR1){
		cout<<"I am sleeping\n";

		//pthread_self() returns the thread id of the calling thread
		threadObj[myThreads::m[pthread_self()]].state = 0;
		status[myThreads::m[pthread_self()]] = 0;
	
	}
	else if(signo==SIGUSR2){
		cout<<"Thanks for waking me up\n";

		//pthread_self() returns the thread id of the calling thread
		threadObj[myThreads::m[pthread_self()]].state = 1;
		status[myThreads::m[pthread_self()]] = 1;
	}

	installSignalHandler();
}

void* doJobs(void *param){
	
	myThreads *t;
	t = (myThreads *) param;

	//m is a map which maps thread id to thread num variable
	myThreads::m[t->tid] = t->threadNum;
	
	installSignalHandler();

	while(1){
	}

	//execution is complete
	threadObj[t->threadNum].state = 2;	
}

void* scheduler(void *param){

	pthread_kill(threadObj[1].tid,SIGUSR1);
	sleep(1);
	pthread_kill(threadObj[4].tid,SIGUSR2);
	sleep(1);
	pthread_kill(threadObj[2].tid,SIGUSR2);
	sleep(1);
	for(int i=0;i<10;i++){
		cout<<status[i]<<'\t';
	}
	cout<<'\n';
	return 0;
}

int main() {
	
	//specifying random seed
	srand((unsigned)time(NULL));

	//Specifying value of N
	int N = 10;

	threadObj = new myThreads[N];

	//create threads
	for(int i=0;i<N;i++){
		threadObj[i].createMe(i);
	}

	//dynamically create status array
	status = new int[N];

	//initializing status array to all zeros
	for(int i=0;i<N;i++){
		status[i] = 0;
	}

	pthread_t 	SchId;

	//create scheduler thread
	pthread_create(&SchId, NULL, scheduler, NULL);
	pthread_join(SchId, NULL);


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

