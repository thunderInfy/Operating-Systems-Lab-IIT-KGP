#include <bits/stdc++.h>
#include <unistd.h>
#include <pthread.h>
using namespace std;

int s[10000], N, pid=0;
int change=0, from=-1, to=-1;

void *report_jobs(void *n) {
	while(1){
		while(!change);
		printf("Change from %d to %d", from, to);
		change = 0;
	}
}

void *schedule_jobs(void *n) {
	while(1) {
		sleep(3);
		s[pid] = 0; from = pid;
		pid = (pid+1)%N; change = 1;
		s[pid] = 1; to = pid;
	}
}

void *do_jobs(void *n) {
	int p = *(int*)n;
	while(1){
		while(!s[p]);
		printf("Thread %d \n",p);
		sleep(1);
	}
}

int main(int argc, char *argv[]) {
	cin>>N;
	pthread_t tid[N], scheduler, reporter;
	int t[N];
	for(int i=0; i<N; i++){
		t[i]=i;
	}

	pthread_create(&scheduler, NULL, schedule_jobs, NULL);
	pthread_create(&reporter, NULL, report_jobs, NULL);

	for(int i=0; i<N; i++){
		pthread_create(&tid[i], NULL, do_jobs, &t[i]);
	}

	while(1);
}

