#include <bits/stdc++.h>
#include <sys/types.h>
#include <sys/stat.h> 
#include <fcntl.h>
#include <unistd.h>

using namespace std;

//parameters for a process
struct procParams{ 
	int   procNumber;	//process index in the order of arrival time
	float arrivalTime;	//arrival time of a process
	float cpuBurst; 	//cpu burst for a process
	float startTime;	//time at which the process is first fed into the CPU
	float endTime;		//the time at which the process is completed
};

struct AvgTurnAroundTimes{
	float fcfs;		//First Come First Served
	float npsjf;	//Non pre-emptive Shortest Job First
	float psjf;		//Pre-emptive Shortest Job First
	float rr;		//Round Robin
	float hrrn;		//Highest response ratio next
};

//get exponential distribution from a uniform random variable R between 0 & 1
float getExpoFromUni(float lambda, float R){
	float out =  (-1.0/lambda)*log(R);
	
	//inter arrival time is between 0 and 10
	if(out>10){
		out = 10;
	}
	return out;
}

//get uniform random variable between 0 and 1
float getUni(){
	return ((double) rand() / (RAND_MAX));
}

//function to generate N processes
procParams* generate_N_processes(int N){
	
	procParams* S;

	S = new procParams[N];

	//specifying some value for lambda
	float lambda = 0.5;
	
	//R is a uniform random variable between 0 and 1
	float R = getUni();

	//the first process
	S[0].procNumber = 
	S[0].arrivalTime = 0;
	S[0].cpuBurst = R*19+1;

	//default start and end times
	S[0].startTime = 0;
	S[0].endTime = S[0].cpuBurst;	


	for(int i=1;i<N;i++){

		S[i].arrivalTime = S[i-1].arrivalTime + getExpoFromUni(lambda, getUni());
		
		R = getUni();
		S[i].cpuBurst = R*19+1;

		//default start and end times
		S[i].startTime = S[i].arrivalTime;
		S[i].endTime = S[i].startTime + S[i].cpuBurst;
	}

	return S;
}

//function to save process parameters in a file named processParams.txt
//if standardOutput is nonzero, then output is sent to STDOUT
void saveProcessParameters(procParams *S, int N, int standardOutput = 0){

	ofstream fout;
	
	if(!standardOutput){
		//open the file
		fout.open("processParams.txt");
		
		//write to the file
		fout<<"Arrival-Time\tCpu-Burst\tStart-Time\tEnd-Time\n";
	}
	else{

		//write to the console
		cout<<"Arrival-Time\tCpu-Burst\tStart-Time\tEnd-Time\n";
	}

	//buffer to store formatted string
	char buffer[200];

	for(int i=0;i<N;i++){

		//store formatted string in buffer
		sprintf(buffer,"%.12f\t%.12f\t%.12f\t%.12f\n",S[i].arrivalTime,S[i].cpuBurst,S[i].startTime,
			S[i].endTime);

		if(!standardOutput)
			fout<<buffer;
		else
			cout<<buffer;
	}

	if(!standardOutput)
		fout.close();
}

//function to copy procParams structure array
void copyProcParams(procParams *B, procParams *A, int N){
	for(int i=0;i<N;i++)
		B[i] = A[i];
}

//comparative function to sort according to arrival times
bool ascendingArrivalOrder(procParams s1, procParams s2){
	return s1.arrivalTime < s2.arrivalTime;
}

//function to simulate FCFS scheduling algorithm
float FCFS(procParams *S, int N){

	//sorting processes on the basis of their arrival times
	sort(S,S+N,ascendingArrivalOrder);

	//starting time for process 0 will be same as arrival time
	S[0].startTime 	= S[0].arrivalTime;
	S[0].endTime 	= S[0].startTime + S[0].cpuBurst;

	for(int i=1;i<N;i++){
		/*
		if S[i].arrivalTime is larger than the earlier process's end time,
		then it should start at the same as arrival time
		*/
		S[i].startTime = max(S[i-1].endTime,S[i].arrivalTime);
		S[i].endTime = S[i].startTime + S[i].cpuBurst;
	}

	float AvgTurnAroundTime = 0;
	for(int i=0;i<N;i++){
		/*
		Turnaround time for a process is the difference between
		its end time and its arrival time
		*/
		AvgTurnAroundTime += S[i].endTime - S[i].arrivalTime;
	}
	AvgTurnAroundTime /= N;

	return AvgTurnAroundTime;
}

// this is an strucure which implements the operator overlading 
struct CompareCPUBursts{ 
	bool operator()(procParams const& p1, procParams const& p2) 
	{ 
		return p1.cpuBurst > p2.cpuBurst; 
	} 
}; 

//function to simulate Non-Preemptive Shortest Job First Algorithm
float NPSJF(procParams *S, int N){

	//sorting processes on the basis of their Arrival Times
	sort(S,S+N,ascendingArrivalOrder);

	//priority queue for shortest job
	priority_queue <procParams, vector<procParams>, CompareCPUBursts> Q;

	float runningTime;

	//starting time for process 0 will be same as its arrival time
	S[0].startTime 	= S[0].arrivalTime;
	S[0].endTime 	= S[0].startTime + S[0].cpuBurst;

	runningTime = S[0].endTime;

	cout<<S[0].cpuBurst<<'\n';

	int i = 1;

	while(S[i].arrivalTime<=runningTime){
		Q.push(S[i]);
		i++;
	}
	procParams p = Q.top();

	cout<<p.cpuBurst<<'\n';
}


int main(){

	//initializing seed
	srand((unsigned)time(NULL));

	//structure pointer to store processes
	procParams* S;

	//structure object for average turnaround time
	AvgTurnAroundTimes A;

	//N processes
	int N = 20;

	//generate 5 processes
	S = generate_N_processes(N);
	
	//save process parameters in a file named processParams.txt
	saveProcessParameters(S,N);

	//copying S into a new structure pointer
	procParams *C;
	C = new procParams[N];
	copyProcParams(C,S,N);

	//getting average turnaround time in member fcfs of structure A
	A.fcfs = FCFS(C,N);

	//copying S again to C
	copyProcParams(C,S,N);

	NPSJF(C,N);
	return 0;
}