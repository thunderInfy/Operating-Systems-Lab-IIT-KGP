#include <cstdio>
#include <signal.h>
#include <unistd.h>

void sig_handler(int signo){
	if(signo==SIGINT){
		printf("Received signal\n");
	}
}

int main(){
	signal(SIGINT, sig_handler);
	while(1){
		sleep(1);
	}
	return 0;
}