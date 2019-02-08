#include <bits/stdc++.h>
#include <unistd.h>
#include <pthread.h>
using namespace std;

int STATUS[10000], N, pid=0, READY[10000];
int change=0, from=-1, to=-1;
pthread_t tid[10000], scheduler, reporter;

void *report_jobs(void *n) {
	while(1){
		while(!change);
		printf("Change from %d to %d", from, to);
		change = 0;
	}
}

void *schedule_jobs(void *n) {
	// kill(tid[0], SIGUSR2);
	while(1) {
		sleep(3);
		kill(tid[READY[pid]], SIGUSR1);
		STATUS[READY[pid]] = 0; 
		from = READY[pid];
		pid = (pid+1)%N; change = 1;
		STATUS[READY[pid]] = 1; 
		to = READY[pid];
		kill(tid[READY[pid]], SIGUSR2);
	}
}

void sig_handler(int signo) {
	if(signo == SIGUSR1){
		STATUS[READY[pid]] = 0;
		printf("Signal received");
	}
	if(signo == SIGUSR2){
		STATUS[READY[pid]] = 1;
		printf("Signal Received");
	}
}

void *do_jobs(void *n) {
	// int p = *(int*)n;
	int p;
	while(1){
		while(!(p=*(int*)n)){
			signal(SIGUSR2, sig_handler);
		}
		sleep(1);
		printf("Thread %d \n",p);
		while((p=*(int*)n)){
			signal(SIGUSR1, sig_handler);
		}
	}
}

int main(int argc, char *argv[]) {
	cin>>N;

	for(int i=0; i<N; i++){
		READY[i]=i;
	}

	for(int i=0; i<N; i++){
		pthread_create(&tid[i], NULL, do_jobs, &STATUS[i]);
	}

	pthread_create(&scheduler, NULL, schedule_jobs, NULL);
	pthread_create(&reporter, NULL, report_jobs, NULL);

	while(1);
}

