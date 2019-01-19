#include<bits/stdc++.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/wait.h>
using namespace std;
#define NUM 50
#define MAX 32767	//32767

void mergeArrays(int arr1[], int arr2[], int n1, int n2, int arr3[]) //Merge function routine
{ 
    int i = 0, j = 0, k = 0; 
  
    while (i<n1 && j <n2) {  		//Check which element smaller and put it in the array
        if (arr1[i] < arr2[j]) 
            arr3[k++] = arr1[i++]; 
        else
            arr3[k++] = arr2[j++]; 
    } 

    while (i < n1) 					//Put the remaining elements if any
        arr3[k++] = arr1[i++]; 

    while (j < n2) 					//Put the remaining elements if any
        arr3[k++] = arr2[j++];

    return; 
} 

int main(){

	int p1[2],p2[2],p3[2],p4[2];	//Define pipes
	if(pipe(p1)==-1 || pipe(p2)==-1 || pipe(p3)==-1 || pipe(p4)==-1){
		printf("Pipe creation failed\n");	//Check for success
		return 0;
	}

	pid_t f1,f2,f3;		//Define process for forks

	f1=fork();			//Create processes with forks
	f2=fork();
	f3=fork();

	if(f1<0 || f2<0 || f3<0){
		printf("Process creation failed!\n");	//Check success of process creation
		return 0;
	}

	if(f1==0){
		if(f2==0){
			if(f3==0){	//Process A

				int a[NUM];							//Declare array
				// printf("Generated numbers A : ");	
				srand(getpid());					//Generate random numbers
				for(int i=0;i<NUM;i++){
					a[i]=rand()%MAX+1;
				}
				sort(a,a+NUM);
				// for(int i=0;i<NUM;i++)printf("%d ",a[i]);	//Print the numbers
				// printf("\n");

				close(p1[0]);
				write(p1[1],a,sizeof(int)*NUM);		//write it to a pipe
				close(p1[1]);

			}
			else {	//Process B

				int b[NUM];
				// printf("Generated numbers B : ");
				srand(getpid());					//Write random numbers to an array
				for(int i=0;i<NUM;i++){
					b[i]=rand()%MAX+1;
				}
				sort(b,b+NUM);
				// for(int i=0;i<NUM;i++)printf("%d ",b[i]);
				// printf("\n");

				close(p2[0]);
				write(p2[1],b,sizeof(int)*NUM);		//write to a pipe
				close(p2[1]);	

			}
		}
		else {
			if(f3==0){	//Process C

				int c[NUM];
				// printf("Generated numbers C : ");
				srand(getpid());					//Generate random numbers in an array
				for(int i=0;i<NUM;i++){
					c[i]=rand()%MAX+1;
				}
				sort(c,c+NUM);
				// for(int i=0;i<NUM;i++)printf("%d ",c[i]);
				// printf("\n");

				close(p3[0]);
				write(p3[1],c,sizeof(int)*NUM);		//write to a pipe
				close(p3[1]);

			}
			else {	//Process D

				// printf("D :");
				int a[NUM],b[NUM],d[2*NUM];		//Read from the pipes and store it in another array
				close(p1[1]);
				read(p1[0],a,NUM*sizeof(int));
				close(p2[1]);
				read(p2[0],b,NUM*sizeof(int));
				mergeArrays(a,b,NUM,NUM,d);		//Merge two sorted arrays
				// for(int i=0;i<2*NUM;i++)printf("%d ",d[i]);
				// printf("\n");

				close(p4[0]);
				write(p4[1],d,2*NUM*sizeof(int));	//write to an array
				close(p4[1]);

			}
		}
	}
	else {
		if(f2==0 && f3==0){	//Process E

			printf("E :");
			int c[NUM],d[2*NUM],e[3*NUM];
			close(p3[1]);						//Read from pipes and store it in another array
			read(p3[0],c,NUM*sizeof(int));
			close(p4[1]);
			read(p4[0],d,2*NUM*sizeof(int));
			mergeArrays(c,d,NUM,2*NUM,e);		//Merge two sorted arrays
			for(int i=0;i<3*NUM;i++)printf("%d ",e[i]);
			printf("\n");

		}
	}

	return 0;

}