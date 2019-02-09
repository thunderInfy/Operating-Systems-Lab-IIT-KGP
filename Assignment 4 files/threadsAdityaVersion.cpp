#include <iostream>
#include <cstdio>
#include <pthread.h>
#include <cstdlib>
#include <ctime>

using namespace std;

void* doJobs(void*);
int* status;

class myThreads{

	private:
		void initType(){
			if((rand() % 2)==0){
				this->type = 'P';
			}
			else{
				this->type = 'C';
			}
		}

	public:

		pthread_t 	tid;
		char 		type;

		void createMe(){
			initType();
			pthread_create(&this->tid, NULL, doJobs, (void *) this);
		}

		void join(){
			pthread_join(this->tid, NULL);
		}

};

void* doJobs(void *param){
	myThreads *t;
	t = (myThreads *) param;
	cout<<t->type<<'\n';
}

int main() {
	
	//specifying random seed
	srand((unsigned)time(NULL));

	//Specifying value of N
	int N = 10;

	//threadObj will be a dynamic array of thread objects
	myThreads *threadObj;
	threadObj = new myThreads[N];

	//dynamically create status array
	status = new int[N];

	//create threads
	for(int i=0;i<N;i++){
		threadObj[i].createMe();
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

