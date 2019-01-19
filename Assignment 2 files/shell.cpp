/*
Group : 6
Name : Aurghya Maiti (16CS10059)
Name : Aditya Rastogi (16CS30042)
*/
#include<bits/stdc++.h>
#include<sys/wait.h>
#include<unistd.h>
#include<fcntl.h>
#include<errno.h>
#include<dirent.h>
#include<sys/stat.h>
#include<time.h>
#include<grp.h>
#include<pwd.h>
#define LEN 1000
#define MAX 100
using namespace std;

void executeExternal(char *input){

	char *args[MAX];
	if(!strcmp(input,"exit\n")){		//If string is quit then quit
		printf("\nExiting from kgp-shell\n");
		exit(0);
	}
	int i=0;
	args[i]=strtok(input," \n");		//Read the filename and individual arguments
	while(args[i]!=NULL){
		// printf("%s",args[i]);
		i++;
		args[i]=strtok(NULL," \n");
	}
	printf("\n");
	pid_t p=fork();
	if(p<0)printf("Terminal process could not be created\n");
	else if(p==0){
		execvp(args[0],args);			//execute the  other program
		printf("Enter a valid program! \n");
		kill(getpid(),SIGTERM);
	}
	else {
		wait(NULL);
		usleep(100000);
	}
}

int main(int argc, char *argv[]){

	printf("Welcome to kgpsh :\n");

	while(1){

		char pwd[LEN]="",command[LEN]="",input[LEN]="",*ptr;

		ptr = getcwd(pwd, sizeof(pwd));		//Get current directory for printing in the bash
		if(ptr==NULL) {
			perror("getcwd ERROR ");
			continue;
		}
		printf("%s$ ", pwd);

		// getting command from user
		fgets (input, 1000, stdin);
		executeExternal(input);
	}
}