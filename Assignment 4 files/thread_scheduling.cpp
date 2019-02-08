#include <bits/stdc++.h>
#include <unistd.h>
#include <pthread.h>
using namespace std;

int s[10000], N, pid=0;

void *schedule_jobs(void *n) {
	while(1) {
		sleep(3);
		s[pid] = 0;
		pid = (pid+1)%N;
		s[pid] = 1;
	}
}

void *do_jobs(void *n) {
	int p = *(int*)n;
	while(!s[p]);
	printf("Thread %d \n",p);
}

int main(int argc, char *argv[]) {
	cin>>N;
	pthread_t tid[N], scheduler;
	int t[N];
	for(int i=0; i<N; i++){
		t[i]=i;
	}

	pthread_create(&scheduler, NULL, schedule_jobs, NULL);

	for(int i=0; i<N; i++){
		pthread_create(&tid[i], NULL, do_jobs, &t[i]);
	}

	while(1);


}