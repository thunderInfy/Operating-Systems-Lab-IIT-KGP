#include<bits/stdc++.h>
#include<unistd.h>
#define MAX 100
#define LEN 100
using namespace std;

int main(){

	while(1){
		char *args[LEN];	//Declare args for storing arguments
		char word[MAX];		//Declare word for reading the filename and arguments
		printf("\nEnter name of program and arguments : (quit to exit)\n");

		fgets(word,MAX,stdin);			//Read the string
		if(!strcmp(word,"quit")){
			return 0;
		}
		
		int i=0;
		args[i]=strtok(word," ");
		while(args[i]!=NULL){
			// printf("%s",args[i]);
			i++;
			args[i]=strtok(NULL," ");
		}

		printf("\n");
		pid_t p=fork();
		if(p==0){
			execvp(args[0],args);
			kill(getpid(),SIGTERM);
		}
		else {
			usleep(100000);
		}
	}
}