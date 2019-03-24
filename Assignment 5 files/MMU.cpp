#include <iostream>
#include <cstdio>		//for perror
#include <cstdlib>		//for exit

using namespace std;

int main(int argc, char* argv[]){

	//parameters (MQ2, MQ3, SM1, SM2)

	if(argc!=5){
		perror("Insufficent number of inputs provided, provide MQ2, MQ3, SM1 and SM2 as command line arguments");
		exit(-1);
	}

	int mq2_id, mq3_id, sm1_id, sm2_id;

	mq2_id = atoi(argv[1]);
	mq3_id = atoi(argv[2]);
	sm1_id = atoi(argv[3]);
	sm2_id = atoi(argv[4]);

	return 0;
}