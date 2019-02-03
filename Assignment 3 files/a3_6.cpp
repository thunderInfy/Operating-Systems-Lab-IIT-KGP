#include <bits/stdc++.h>
#include <sys/types.h>
#include <sys/stat.h> 
#include <fcntl.h>
#include <unistd.h>

using namespace std;

//parameters for a process
struct procParams{ 
	int   procNumber;	//process index in the order of arrival time
	double arrivalTime;	//arrival time of a process
	double cpuBurst; 	//cpu burst for a process
	double startTime;	//time at which the process is first fed into the CPU
	double endTime;		//the time at which the process is completed
};

//structure to hold values of average turn around times for different strategies
struct AvgTurnAroundTimes{
	double fcfs;		//First Come First Served
	double npsjf;		//Non pre-emptive Shortest Job First
	double psjf;		//Pre-emptive Shortest Job First
	double rr;			//Round Robin
	double hrrn;		//Highest response ratio next
};

//get exponential distribution from a uniform random variable R between 0 & 1
double getExpoFromUni(double lambda, double R){
	double out =  (-1.0/lambda)*log(R);
	
	//inter arrival time is between 0 and 10
	if(out>10){
		out = 10;
	}
	return out;
}

//get uniform random variable between 0 and 1
double getUni(){
	return ((double) rand() / (RAND_MAX));
}

//function to generate N processes
procParams* generate_N_processes(int N){
	
	procParams* S;

	S = new procParams[N];

	//specifying some value for lambda
	double lambda = 0.5;
	
	//R is a uniform random variable between 0 and 1
	double R = getUni();

	//the first process
	S[0].procNumber = 0;
	S[0].arrivalTime = 0;
	S[0].cpuBurst = R*19+1;

	//default start and end times
	S[0].startTime = 0;
	S[0].endTime = S[0].cpuBurst;	


	for(int i=1;i<N;i++){

		S[i].procNumber = i;
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
		fout<<"S.No.\tArrival-Time\tCpu-Burst\tStart-Time\tEnd-Time\n";
	}
	else{

		//write to the console
		cout<<"S.No.\tArrival-Time\tCpu-Burst\tStart-Time\tEnd-Time\n";
	}

	//buffer to store formatted string
	char buffer[200];

	for(int i=0;i<N;i++){

		//store formatted string in buffer
		sprintf(buffer,"%d\t%.12f\t%.12f\t%.12f\t%.12f\n",S[i].procNumber,S[i].arrivalTime,S[i].cpuBurst,S[i].startTime,
			S[i].endTime);

		if(!standardOutput)
			fout<<buffer;
		else
			cout<<buffer;
	}

	if(!standardOutput)
		fout.close();
}

//function to calculate average turnaround time
double calcAvgTurnAroundTime(procParams* S, int N){
	double AvgTurnAroundTime = 0;
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

//function to copy procParams structure array
void copyProcParams(procParams *B, procParams *A, int N){
	for(int i=0;i<N;i++)
		B[i] = A[i];
}

//comparative function to sort according to arrival times
bool ascendingArrivalOrder(procParams s1, procParams s2){
	return s1.arrivalTime < s2.arrivalTime;
}

//comparative function to sort according to CPU bursts
bool ascendingCPUBursts(procParams s1, procParams s2){
	return s1.cpuBurst < s2.cpuBurst;
}

//function to simulate FCFS scheduling algorithm
double FCFS(procParams *S, int N){

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

	return calcAvgTurnAroundTime(S,N);

}

// this is an strucure which implements the operator overlading 
struct CompareCPUBursts{ 
	bool operator()(procParams const& p1, procParams const& p2) 
	{ 
		return p1.cpuBurst > p2.cpuBurst; 
	} 
}; 

//function to simulate Non-Preemptive Shortest Job First Algorithm
double NPSJF(procParams *S, int N){

	/*
	in this way, if any two processes have the same arrival times, 
	then, they will be sorted according to their CPU bursts
	*/

	//sorting processes on the basis of their CPU bursts
	sort(S,S+N,ascendingCPUBursts);

	//sorting processes on the basis of their Arrival Times
	stable_sort(S,S+N,ascendingArrivalOrder);

	//priority queue for shortest job
	priority_queue <procParams, vector<procParams>, CompareCPUBursts> Q;

	//variable to store the timestamp
	double runningTime = 0;

	int i = 0;

	while(1){
		/*
		adding all those processes to the priority queue which have 
		arrived till now
		*/
		while(S[i].arrivalTime<=runningTime){
			Q.push(S[i]);
			i++;
			if(i>=N){
				break;
			}
		}
		if(i>=N){
			break;
		}
		if(!Q.empty()){
			//if Q isn't empty, then pop out a process and run it
			procParams p = Q.top();
			S[p.procNumber].startTime = runningTime;
			runningTime += p.cpuBurst;
			S[p.procNumber].endTime = runningTime;
			Q.pop();
		}
		else{
			runningTime = S[i].arrivalTime;
		}
	}
	while(!Q.empty()){
		//run all the remaining processes left in the priority queue
		procParams p = Q.top();
		S[p.procNumber].startTime = runningTime;
		runningTime += p.cpuBurst;
		S[p.procNumber].endTime = runningTime;
		Q.pop();
	}

	return calcAvgTurnAroundTime(S,N);
}

//function to simulate Preemptive Shortest Job First Algorithm
double PSJF(procParams *S, int N){

	/*
	in this way, if any two processes have the same arrival times, 
	then, they will be sorted according to their CPU bursts
	*/

	//sorting processes on the basis of their CPU bursts
	sort(S,S+N,ascendingCPUBursts);

	//sorting processes on the basis of their Arrival Times
	stable_sort(S,S+N,ascendingArrivalOrder);

	//priority queue for shortest job
	priority_queue <procParams, vector<procParams>, CompareCPUBursts> Q;

	//variable to store the timestamp
	double runningTime = 0;

	//variable to store index of running process
	int runningProc = -1;

	for(int i=0;i<N;i++){
		//the i-th process arrives

		//current time is same as the arrival time of the i-th process
		runningTime = S[i].arrivalTime;
		
		if(runningProc >= 0){
			//some process was running earlier
			
			if(runningTime >= (S[runningProc].startTime + S[runningProc].cpuBurst)){
				//the running process has ended
				S[runningProc].endTime = S[runningProc].startTime + S[runningProc].cpuBurst;
				runningTime = S[runningProc].endTime;
				runningProc = -1;
				i--;
			}
			else{
				//the running process is still going on
				//update its cpuBurst
				S[runningProc].cpuBurst-= (runningTime - S[runningProc].startTime);
				Q.push(S[runningProc]);
				Q.push(S[i]);
			}
		}
		else{
			Q.push(S[i]);
		}

		do{
			//run the process with the minimum CPU Burst
			if(!Q.empty()){
				procParams p = Q.top();
				p.startTime = runningTime;
				runningProc = p.procNumber;
				S[runningProc] = p;
				S[runningProc].endTime = S[runningProc].startTime + S[runningProc].cpuBurst;
				runningTime = S[runningProc].endTime;
				Q.pop();
			}
		}while(i==N-1 && !Q.empty());
	}

	return calcAvgTurnAroundTime(S,N);
}

int main(){

	//initializing seed
	srand((unsigned)time(NULL));

	//structure pointer to store processes
	procParams* S;

	//structure object for average turnaround time
	AvgTurnAroundTimes A;

	//N processes
	int N = 10;

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

	//getting average turnaround time in member npsjf of structure A
	A.npsjf = NPSJF(C,N);

	//copying S again to C
	copyProcParams(C,S,N);

	//getting average turnaround time in member psjf of structure A
	A.psjf = PSJF(C,N);

	return 0;
}