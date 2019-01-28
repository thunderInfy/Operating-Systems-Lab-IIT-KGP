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

// break a string according to a delimiter
int breakString(char **list,char *str,const char *delim){
	int i=0;
	list[i]=strtok(str,delim);
	while(list[i]!=NULL){
		i++;
		list[i]=strtok(NULL,delim);
	}
	return i;
}

// find the index of <
int findInput(char *input){
	for(int i=0;input[i]!='\0';i++){
		if(input[i]=='<')
			return i;
	}
	return -1;
}

// find the index of >
int findOutput(char *input){
	for(int i=0;input[i]!='\0';i++){
		if(input[i]=='>')
			return i;
	}
	return -1;
}

// execute external command
void executeExternal(char *input){

	char *args[MAX];

	//If string is exit then exit
	if(!strcmp(input,"exit")){		
		printf("\nExiting from kgp-shell\n");
		exit(0);
	}

	breakString(args,input," \n");

	//execute the  other program
	execvp(args[0],args);
	printf("Error in executing the file! \n");
	kill(getpid(),SIGTERM);

}

void executeInputOutput(char *input){

	char *args[MAX];
	char *files[MAX];

	int i=0;

	//in is the index of < character
	int in=findInput(input);

	//out is the index of > character
	int out=findOutput(input);
	
	// break input according &, <, > and \n and get size of args in i
	i=breakString(args,input,"&<>\n");
	
	pid_t p=0;
	
	if(p==0){
		if(in==-1 && out==-1){
			if(i<=1){
				executeExternal(args[0]);
			}
			else{ 
				printf("Error encountered! Unexpected additional parameters\n");
			}
		}
		else if(in>0 && out==-1){
			if(i==2){
				int f=breakString(files,args[1]," \n");

				if(f!=1){
					printf("Input file error.");
					return;
				}
				int ifd = open(files[0], O_RDONLY);
				if(ifd<0){
					printf("Error when opening input file.\n");
					return;
				}

				//redirect STDIN to the mentioned input file
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

				//redirect STDOUT to the mentioned output file
				close(1);
				dup(ofd);
				close(ofd);
			}
		}
		else if(in>0 && out>0 && i==3){
			int ifd,ofd,f;
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
				ofd = open(files[0], O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

				f=breakString(files,args[1]," \n");
				if(f!=1){
					printf("Error in output file\n");
					return;
				}
				ifd = open(files[0], O_RDONLY);				
			}

			//redirect STDIN and STDOUT to mentioned files

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
        if((pd=fork()) == 0) { 
        	//this is the child process
        	//read from the previous pipe if it exists
        	if(i!=0){
        		
        		//it doesn't write to the previous pipe
				close(p[i-1][1]);

        		//redirect STDIN to read end of the previous pipe
        		dup2(p[i-1][0], 0);
				
				//close the read pipe end
				close(p[i-1][0]); 
        	}
        	
        	//write to the next pipe if it exists
        	if(i!=N-1){
        		
        		//it doesn't read from the next pipe
				close(p[i][0]);
        		
				//redirects STDOUT to write end of the next pipe
        		dup2(p[i][1], 1);

        		//close the write pipe end
				close(p[i][1]);	
        	}
			for(int j=0;j<N-1;j++){	
			// closes all pipes
				close(p[j][0]);
				close(p[j][1]);
			}

        	//execute the i-th command
        	executeInputOutput(args[i]);

        	//exit from the child process	
            exit(0); 
        }
		else {
			//wait for a while
			usleep(10000);

			for(int j=0;j<i;j++){	// closes all pipes
				close(p[j][0]);
				close(p[j][1]);
			}
		}
    }
	while(wait(NULL)>0);
	exit(0);
}

int main(int argc, char *argv[]){

	//print welcome string in green colour
	cout<<"\033[1;32mWelcome to kgpsh :\033[0m\n";

	while(1){

		//strings for I/O and for holding other data
		char pwd[LEN]="";
		char input[LEN]="";
		char *ptr;

		//Get current directory for printing in the bash
		ptr = getcwd(pwd, sizeof(pwd));		

		//error in getcwd
		if(ptr==NULL) {
			perror("Error in getting the current directory");
			continue;
		}

		//print present working directory in blue colour
		cout<<"\033[1;34m"<<pwd<<"\033[0m"<<"$ ";

		//get command from user
		fgets(input, 1000, stdin);	

		//check for exit condition
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

		//flag to check for & in the command
		int backFlag = 0;
		int j;

		//checking & character followed by any number of spaces
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

		if(fork()==0){
			executePipe(args, i);
		}
		else {
			//waiting for child process to exit if backflag is 0	
			if(backFlag==0){
				while(waitpid(-1,NULL,0)>0);
			}
		}
	}
}