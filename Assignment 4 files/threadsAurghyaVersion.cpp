#include <bits/stdc++.h>
#include <iostream>
#include <cstdio>
#include <pthread.h>
#include <cstdlib>
#include <ctime>
#include <signal.h>
#include <unistd.h>
#include <map>

using namespace std;

#define CPS CLOCKS_PER_SEC
#define SLEEP 0
#define RUN 1
#define TERMINATE 2

void* doJobs(void*);
void sleepOrWake(int);

int completed = 0;

int N, MAX;
vector<int> buffer;

typedef struct _contextSwitch {
	int flag;
	int from;
	int to;
}contextSwitch;

struct STATUS{
	int* 			status;
	contextSwitch 	change;
	int 			threadTerminated;
};

STATUS S;


//class for handling thread functions
class myThreads{

	public:

		pthread_t 	tid;				//thread id
		char 		type;				//thread type 'P' or 'C'
		int 		threadNum;			//thread index in the global array
		int 		state;				//state = 0 means sleep, 1 means awake and 2 means complete
		static map <pthread_t, int> m;	//map to get threadNum from tid

	private:

		//initializing type of thread with 'P' or 'C' each having equal probability
		void initType(){
			if((rand() % 2)==0){
				this->type = 'P';
			}
			else{
				this->type = 'C';
			}
		}

	public:

		//create thread
		void createMe(int index){
			//give thread a type
			this->initType();

			//give it a index, same as received parameter
			this->threadNum = index;

			//initially, the thread will sleep
			this->state = 0;

			//create the thread
			pthread_create(&this->tid, NULL, doJobs, (void *) this);
		}

		void join(){

			//join thread
			pthread_join(this->tid, NULL);
		}
};

myThreads *threadObj;				//threadObj will be a dynamic array of thread objects
map <pthread_t, int> myThreads::m;	//declaring static member map of the above class

//function to install signal handlers
void installSignalHandler(){
	//installing signal handlers here
	signal(SIGUSR1, sleepOrWake);
	signal(SIGUSR2,	sleepOrWake);
}

//the function which handles signals
void sleepOrWake(int signo){

	if(signo==SIGUSR1){
		// cout<<"I am sleeping "<<threadObj[myThreads::m[pthread_self()]].threadNum<<"\n";

		//pthread_self() returns the thread id of the calling thread
		threadObj[myThreads::m[pthread_self()]].state = SLEEP;
		
		//updating STATUS data structure
		S.status[myThreads::m[pthread_self()]] = SLEEP;
	
	}
	else if(signo==SIGUSR2){
		// cout<<"Thanks for waking me up "<<threadObj[myThreads::m[pthread_self()]].threadNum<<"\n";

		//pthread_self() returns the thread id of the calling thread
		threadObj[myThreads::m[pthread_self()]].state = RUN;
		
		//updating STATUS data structure
		S.status[myThreads::m[pthread_self()]] = RUN;
	}

	installSignalHandler();
}

void* doJobs(void *param){
	
	myThreads *t;
	t = (myThreads *) param;

	//m is a map which maps thread id to thread num variable
	// myThreads::m[t->tid] = t->threadNum;
	
	installSignalHandler();

	int num = t->threadNum;

	if(threadObj[num].type == 'P'){
		int x=5;
		while(x){
			while(!(threadObj[num].state));
			if(buffer.size()==MAX){
				threadObj[num].state=0;
				S.status[num]=0;
			}
			else {
				buffer.push_back(rand()%100);
				x--;
				usleep(100000);
			}
		}
		threadObj[num].state = TERMINATE;
		S.status[num] = TERMINATE;
	}
	else if(threadObj[num].type == 'C'){
		//while(threadObj[num].state!=TERMINATE){
		while(!completed){
			//while(!(threadObj[num].state));
			
			if(threadObj[num].state!=SLEEP){

				if(buffer.size()==0){
					threadObj[num].state = 0;
					S.status[num] = 0;
				}
				else {
					buffer.pop_back();
					usleep(100000);
				}
			}
		}
	}

	threadObj[num].state = TERMINATE;
	// cout<<"I am terminated "<<num<<'\n';

	return 0;
}

int count_complete(){
	for(int i=0; i<N; i++){
		if(threadObj[i].type=='P' && threadObj[i].state!=2)return 0;
	}
	return (buffer.size()==0);
}

void* scheduler(void *param){

	//clock_t start_time;

	while(1) {
		sleep(1);
		//cout<<" "<<change.to<<" "<<threadObj[change.to].tid<<" "<<threadObj[change.to].threadNum<<" ";
		

		if(threadObj[S.change.to].state!=TERMINATE)
			pthread_kill(threadObj[S.change.to].tid, SIGUSR2);

		sleep(2);
		S.change.from = S.change.to;

		S.change.to = (S.change.to+1)%N;
		while(S.status[S.change.to]==2)
			S.change.to = (S.change.to+1)%N;

		if(threadObj[S.change.from].state!=TERMINATE)
			pthread_kill(threadObj[S.change.from].tid, SIGUSR1);

		S.change.flag = 1;

		if(count_complete()){
			for(int i=0;i<N;i++){
				threadObj[i].state = TERMINATE;
				S.status[i] = TERMINATE;
			}
			break;
		}
	}

	return NULL;
}

int *previous;
void report(){
	for(int i=0; i<N; i++){
		if(S.status[i]!=previous[i]){
			string curr="",prev="";
			
			if(S.status[i] == 0)
				curr = "SLEEP";
			else if(S.status[i] == 1)
				curr = "RUNNING";
			else if(S.status[i] == 2)
				curr = "TERMINATED";

			if(previous[i] == 0)
				prev = "SLEEP";
			else if(previous[i] == 1)
				prev = "RUNNING";
			else if(previous[i] == 2)
				prev = "TERMINATED";

			previous[i]=S.status[i];

			cout<<"Buffer Size : "<<buffer.size()<<endl;
			cout<<"Change of "<<i<<" from "<<prev<<" to "<<curr<<endl;
		}
	}
}

void* reporter(void *param){

	previous = new int[N];
	for(int i=0; i<N; i++)previous[i]=0;
	while(1){
		report();
		// while(!S.change.flag);
		// S.change.flag = 0;
		// cout<<buffer.size()<<endl;
		// cout<<"Context Switch from "<<threadObj[S.change.from].type<<S.change.from<<" to "<<threadObj[S.change.to].type<<S.change.to<<endl;
		if(count_complete()){
			completed = 1;
			cout<<"Process Complete";
			break;
			// return NULL;
		}
	}
}

int main() {
	
	//specifying random seed
	srand((unsigned)time(NULL));

	//Specifying value of N
	N = 5;

	//Specifying value of MAX
	MAX = 50;

	//initializing status change member
	S.change.flag = 0;					//flag is 1 iff context switch happens
	S.change.from = 0;					//context switch from thread index
	S.change.to = 0;					//context switch to thread index

	//declare N threads
	threadObj = new myThreads[N];

	//create threads
	for(int i=0;i<N;i++){
		threadObj[i].createMe(i);
	}

	//initializing the map from thread id to thread index
	for(int i=0;i<N;i++){
		myThreads::m[threadObj[i].tid]=threadObj[i].threadNum;
	}

	//dynamically create status array of STATUS data structure's object S
	S.status = new int[N];

	//initializing status array to all zeros
	for(int i=0;i<N;i++){
		S.status[i] = 0;
	}

	//thread ids for scheduler and reporter threads
	pthread_t SchId;
	pthread_t RepId;

	//create scheduler thread
	pthread_create(&SchId, NULL, scheduler, NULL);

	//create reporter thread
	pthread_create(&RepId, NULL, reporter, NULL);

	//wait for reporter thread to join main thread
	pthread_join(RepId, NULL);

	//wait for scheduler thread to join main thread
	pthread_join(SchId, NULL);

	//all producer threads have terminated, steady state reached
	cout<<"\nOnly consumer threads are left!\n";

	//wait for all threads to join the main thread
	for(int i=0;i<N;i++){
		threadObj[i].join();
	}

	//delete the dynamic array of threadObj
	delete threadObj;

	//delete the dynamic array of status
	delete S.status;

	return 0;
}
