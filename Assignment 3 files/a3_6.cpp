#include <bits/stdc++.h>

using namespace std;

struct procParams{ 
	float arrivalTime;
	float cpuBurst; 
	float startTime;
	float endTime;
};

float getExpoFromUni(float lambda, float R){
	return (-log(R) / lambda);
}

procParams* generate_n_processes(int N){
	
	procParams* S;

	S = new procParams[N];

	int i;

	//specifying value of lambda
	float lambda = 5;
	float R;

	//R is a uniform random variable between 0 and 1
	R = ((double) rand() / (RAND_MAX));

	//the first process
	S[0].arrivalTime = 0;
	S[0].cpuBurst = R*19+1;
	S[0].startTime = S[0].arrivalTime;
	S[0].endTime = S[0].startTime + S[0].cpuBurst;


	for(i=1;i<N;i++){

		R = ((double) rand() / (RAND_MAX));
		S[i].arrivalTime = S[i-1].arrivalTime + getExpoFromUni(lambda, R);
		
		R = ((double) rand() / (RAND_MAX));
		S[i].cpuBurst = R*19+1;

		S[i].startTime = S[i].arrivalTime;
		S[i].endTime = S[i].startTime + S[i].cpuBurst;
	}

	return S;
}

void printProcessParameters(procParams *S, int N){
	int i = 0;

	cout<<"Arrival-Time\tCpu-Burst\tStart-Time\tEnd-Time\n";
	for(;i<N;i++){
		printf("%.12f\t%.12f\t%.12f\t%.12f\n",S[i].arrivalTime,S[i].cpuBurst,S[i].startTime,
			S[i].endTime);
	}
}



bool compareArrival(procParams s1, procParams s2){
	return s1.arrivalTime < s2.arrivalTime;
}


double FCFS(procParams *S, int N){

	sort(S,S+N,compareArrival);
	
	
}


int main(){

	//initializing seed
	srand(time(0));

	procParams* S;

	int N = 5;

	S = generate_n_processes(N);
	printProcessParameters(S,N);


	return 0;
}