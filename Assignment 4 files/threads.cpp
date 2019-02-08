// C code to print 1 2 3 infinitely using pthread 
#include <bits/stdc++.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
using namespace std; 

// Declaration of thread condition variables 
pthread_cond_t cond[1000000]; 

// mutex which we are going to use avoid race condition. 
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER; 

// done is a global variable which decide which waiting thread should be scheduled 
int done = 0, N=0; 

// Thread function 
void *print(void *n) 
{ 
        while(1) { 
                // acquire a lock 
                pthread_mutex_lock(&lock); 

                int num = (int)*(int *)n;
                
                if (done != num) {  
                        pthread_cond_wait(&cond[num], &lock);

                } 
                // done is equal to n, then print n
                printf("%d ", num);
                usleep(1000); 

                // next process
                done = (done+1)%N;
                pthread_cond_signal(&cond[done]);
                
                // Finally release mutex 
                pthread_mutex_unlock(&lock); 
        } 

        return NULL; 
} 

// Driver code 
int main() 
{ 
        cin >> N;
        pthread_t tid[N];
        int n[N];

        for(int i=0; i<N; i++){
                cond[i]=PTHREAD_COND_INITIALIZER;
                n[i]=i;
        } 

        for(int i=0; i<N; i++){
                pthread_create(&tid[i], NULL, print, (void *)&n[i]);
        }
        
        // infinite loop to avoid exit of a program/process 
        while(1); 
        
        return 0; 
} 
