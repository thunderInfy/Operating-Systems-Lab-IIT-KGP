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

int* status;

// struct STATUS{
// 	int* status;
// 	int  statusChanged;
// 	int  threadTerminated;
// };

int N, MAX;
vector<int> buffer;

typedef struct _contextSwitch {
	int flag;
	int from;
	int to;
}contextSwitch;
contextSwitch change;

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

		void exit(){
			//pthread_cancel(this->tid, NULL);
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
		cout<<"I am sleeping "<<threadObj[myThreads::m[pthread_self()]].threadNum<<"\n";

		//pthread_self() returns the thread id of the calling thread
		threadObj[myThreads::m[pthread_self()]].state = SLEEP;
		status[myThreads::m[pthread_self()]] = SLEEP;
	
	}
	else if(signo==SIGUSR2){
		cout<<"Thanks for waking me up "<<threadObj[myThreads::m[pthread_self()]].threadNum<<"\n";

		//pthread_self() returns the thread id of the calling thread
		threadObj[myThreads::m[pthread_self()]].state = RUN;
		status[myThreads::m[pthread_self()]] = RUN;
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
				status[num]=0;
			}
			else {
				buffer.push_back(rand()%100);
				x--;
				usleep(100000);
			}
		}
		threadObj[num].state = TERMINATE;
		status[num] = TERMINATE;
	}
	else if(threadObj[num].type == 'C'){
		//while(threadObj[num].state!=TERMINATE){
		while(!completed){
			//while(!(threadObj[num].state));
			
			if(threadObj[num].state!=SLEEP){

				if(buffer.size()==0){
					threadObj[num].state = 0;
					status[num] = 0;
				}
				else {
					buffer.pop_back();
					usleep(100000);
				}
			}
		}
	}

	threadObj[num].state = TERMINATE;
	cout<<"I am terminated "<<num<<'\n';

	return 0;
}

int count_complete(){
	for(int i=0; i<N; i++){
		if(threadObj[i].type=='P' && threadObj[i].state!=2)return 0;
	}
	return (buffer.size()==0);
}

void* scheduler(void *param){

//	clock_t start_time; 
	while(1) {
		sleep(1);
		//cout<<" "<<change.to<<" "<<threadObj[change.to].tid<<" "<<threadObj[change.to].threadNum<<" ";
		

		if(threadObj[change.to].state!=TERMINATE)
			pthread_kill(threadObj[change.to].tid, SIGUSR2);

		// start_time = clock();
		// sleep(1);
		// while((clock()-start_time)/CPS < 3 && threadObj[change.to].tid==1);
		sleep(2);
		change.from = change.to;

		change.to = (change.to+1)%N;
		while(status[change.to]==2)
			change.to = (change.to+1)%N;

		if(threadObj[change.from].state!=TERMINATE)
			pthread_kill(threadObj[change.from].tid, SIGUSR1);
		
		// if(count_complete()){
		// 	for(int i=0; i<N; i++){
		// 		if(threadObj[i].type == 'C'){
		// 			threadObj[i].state = TERMINATE;
		// 			status[i] = TERMINATE;
		// 		}
		// 	}
		// 	return NULL;
		// }

		change.flag = 1;

		if(count_complete()){
			for(int i=0;i<N;i++){
				threadObj[i].state = TERMINATE;
				status[i] = TERMINATE;
			}
			break;
		}
	}

	return NULL;
}

void* reporter(void *param){

	while(1){
		while(!change.flag);
		change.flag = 0;
		cout<<buffer.size()<<endl;
		cout<<"Context Switch from "<<threadObj[change.from].type<<change.from<<" to "<<threadObj[change.to].type<<change.to<<endl;
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
	N = 5;MAX = 50;
	change.flag = 0;
	change.from = 0;
	change.to = 0;

	threadObj = new myThreads[N];

	//create threads
	for(int i=0;i<N;i++){
		threadObj[i].createMe(i);
	}

	for(int i=0;i<N;i++){
		myThreads::m[threadObj[i].tid]=threadObj[i].threadNum;
	}

	//dynamically create status array
	status = new int[N];

	//initializing status array to all zeros
	for(int i=0;i<N;i++){
		status[i] = 0;
	}

	pthread_t SchId;
	pthread_t RepId;
	//create scheduler thread
	pthread_create(&SchId, NULL, scheduler, NULL);

	//create reporter thread
	pthread_create(&RepId, NULL, reporter, NULL);

	cout<<"reached here! 0\n";

	pthread_join(RepId, NULL);

	cout<<"reached here! 1\n";


	pthread_join(SchId, NULL);


	cout<<"reached here! 2\n";


	for(int i=0;i<N;i++){
		cout<<threadObj[i].threadNum<<'\t'<<threadObj[i].state<<'\n';
		threadObj[i].state = TERMINATE;
	}

	cout<<"\nOnly consumer threads ae left!\n";

	//join threads
	for(int i=0;i<N;i++){
		threadObj[i].join();
	}

	cout<<"reached here! 3\n";


	//delete the dynamic array of threadObj
	delete threadObj;

	//delete the dynamic array of status
	delete status;

	return 0;
}
