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
#define EXIT_FLAG 0
using namespace std;

int breakString(char **list,char *str,char *delim){	// break a string according to a delimiter
	int i=0;
	list[i]=strtok(str,delim);
	while(list[i]!=NULL){
		i++;
		list[i]=strtok(NULL,delim);
	}
	return i;
}

int findInput(char *input){				// find the index of <
	for(int i=0;i<strlen(input);i++){
		if(input[i]=='<')
			return i;
	}
	return -1;
}

int findOutput(char *input){			// find the index of >
	for(int i=0;i<strlen(input);i++){
		if(input[i]=='>')
			return i;
	}
	return -1;
}

void executeExternal(char *input){		// execute external command

	char *args[MAX];
	if(!strcmp(input,"exit")){		//If string is quit then quit
		printf("\nExiting from kgp-shell\n");
		exit(0);
	}

	breakString(args,input," \n");

	execvp(args[0],args);			//execute the  other program
	printf("Error in executing the file! \n");
	kill(getpid(),SIGTERM);

}

void executeInputOutput(char *input){
	char *args[MAX];
	char *files[MAX];
	int i=0;

	int in=findInput(input);
	int out=findOutput(input);
	// printf("%d %d ",in,out);
	i=breakString(args,input,"<>\n");
	// printf(" %d ",i);
	// printf("%s %s ",args[1],args[0]);

	// pid_t p=fork();
	pid_t p=0;
	if(p==0){
		if(in==-1 && out==-1){
			if(i<=1){
				executeExternal(args[0]);
			}
			else printf("Error encountered!\n");
		}
		else if(in>0 && out==-1){
			if(i==2){
				int f=breakString(files,args[1]," \n");
				// printf("%s %s %d\n",files[0],files[1],f);
				if(f!=1){
					printf("Input file error.");
					return;
				}
				int ifd = open(files[0], O_RDONLY);
				if(ifd<0){
					printf("Error when opening input file.\n");
					return;
				}
				close(0);
				dup(ifd);
				close(ifd);
			}
			else printf("Error!\n");
		}
		else if(in==-1 && out>0){
			if(i==2){
				int f=breakString(files,args[1]," \n");
				if(f!=1){
					printf("Output file error!");
					return;
				}
				int ofd = open(files[0], O_CREAT | O_WRONLY | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
				if(ofd<0){
					printf("Error when opening output file.\n");
					return;
				}
				close(1);
				dup(ofd);
				close(ofd);
			}
		}
		else if(in>0 && out>0 && i==3){
			int ifd,ofd,f;
			// printf("in out identified ");
			if(in>out){
				f=breakString(files,args[1]," \n");
				if(f!=1){
					printf("Error in output file\n");
					return;
				}
				ofd = open(files[0], O_CREAT | O_WRONLY | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

				f=breakString(files,args[2]," \n");
				if(f!=1){
					printf("Error in output file\n");
					return;
				}
				ifd = open(files[0], O_RDONLY);
			}
			else if(in<out){
				f=breakString(files,args[2]," \n");
				if(f!=1){
					printf("Error in output file\n");
					return;
				}
				printf("%s ",files[0]);
				ofd = open(files[0], O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

				f=breakString(files,args[1]," \n");
				if(f!=1){
					printf("Error in output file\n");
					return;
				}
				// printf("%s ",files[0]);
				ifd = open(files[0], O_RDONLY);				
			}
			close(0);
			dup(ifd);
			close(1);
			dup(ofd);
			close(ifd);
			close(ofd);
		}
		else return;
		executeExternal(args[0]);
	}
	return;
	exit(0);
}

void executePipe(char** args, int N){
	//define N-1 pipes
	int p[N-1][2];

	for(int i=0;i<N-1;i++){
		//create pipe
		if(pipe(p[i])<0){
			printf("Pipe creation failed!");
			return;
		}	
	}

	//create N child processes
	pid_t pd;
	for(int i=0;i<N;i++) { 
		// printf("%d",i);
        if((pd=fork()) == 0) { 
        	//this is the child process
        	//read from the previous pipe if it exists
        	if(i!=0){
        		//redirects STDIN to read end of the previous pipe
        		dup2(p[i-1][0], 0); 
        	}
        	
        	//write to the next pipe if it exists
        	if(i!=N-1){
        		//redirects STDOUT to write end of the next pipe
        		dup2(p[i][1], 1);	
        	}
			for(int j=0;j<N-1;j++){	// closes all pipes
				close(p[j][0]);
				close(p[j][1]);
			}

        	//execute the i-th command
        	executeInputOutput(args[i]);

        	//exit from the child process	
            exit(0); 
        }
		else {
			usleep(10000);
		}
    }
	waitpid(-1,NULL,0);
	exit(0);
    // wait for the child process if backFlag is 0, otherwise not
	// if(backFlag==0)
	// 	wait(NULL); 
}

int main(int argc, char *argv[]){

	//print welcome string in green colour
	cout<<"\033[1;32mWelcome to kgpsh :\033[0m\n";

	while(1){

		char pwd[LEN]="";
		char command[LEN]="";
		char input[LEN]="";
		char *ptr;

		//Get current directory for printing in the bash
		ptr = getcwd(pwd, sizeof(pwd));		

		//error in getcwd
		if(ptr==NULL) {
			perror("getcwd error");
			continue;
		}
		//printf("%s$ ", pwd);

		//print present working directory in blue colour
		cout<<"\033[1;34m"<<pwd<<"\033[0m"<<"$ ";

		//get command from user
		fgets(input, 1000, stdin);	

		if(strlen(input)>=4){
			if(input[0]=='e' && input[1]=='x' && input[2]=='i' && input[3]=='t'){
				int k = 4;
				while(input[k]==' '){
					k++;
				}
				if(input[k]=='\n'){
					printf("Exiting from kgp-shell...\n");	
					exit(0);
				}
			}
		}

		//for the pipe commands' arguments
		char *args[MAX];
		int i=0;

		//checking the existence of pipes in the entered command

		//separating individual commands by |
		args[i]=strtok(input,"|");
	
		while(args[i]!=NULL){
			i++;
			args[i]=strtok(NULL,"|");
		}

		// regex b("(.)*&( )*\n");

		int backFlag = 0;

		int j;
		for(int k=0;input[k]!='\0';k++){
			if(input[k]=='&'){
				j = k;
				j++;
				while(input[j]==' '){
					j++;
				}
				if(input[j]=='\n'){
					backFlag = 1;
					input[k++] = '\n';
					input[k] = '\0';
				}
			}
		}

		if(fork()==0){
			executePipe(args, i);
			kill(getpid(),SIGTERM);
		}
		else {
			// printf("%d ",backFlag);
			usleep(90000);
			if(backFlag==0){
				while(wait(NULL)>0);
			}
		}
	}
}