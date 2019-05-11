#include "myfs.h"

int main(){	

	init(100, 1, (char *)("root"));

	int fd = my_open("code.py");

	char buf[] = "abcabc";

	//cout<<strlen(buf)<<'\n';
	my_write(fd, buf, strlen(buf));

	//my_cat(fd);

	char mybuffer[10000];
	my_read(fd,mybuffer, 2);

	//cout<<"\n\n\n\n"<<mybuffer<<'\n';

	my_read(fd,mybuffer, 2);

	//cout<<"\n\n\n\n"<<mybuffer<<'\n';

	if(!my_copy("/home/thunderinfy/Desktop/OS/os/qwerty.txt",fd)){
		perror("error in copying!");
	}

	if(!my_copy(fd,"/home/thunderinfy/Desktop/OS/Ass4_6.cpp")){
		perror("error in copying!");
	}

	my_cat(fd);

	if(!my_copy("/home/thunderinfy/Desktop/OS/os/output.cpp",fd)){
		perror("error in copying!");
	}


	//my_read(fd, mybuffer, 10000);
	//cout<<"\n\n\n\n"<<mybuffer<<'\n';

	my_close(fd);

	my_mkdir("home");
	my_mkdir("trash");
	my_mkdir("home/Desktop");
	my_mkdir("home/Desktop/q");
	my_mkdir("home/Desktop/w");
	my_mkdir("home/Music");
	my_mkdir("home/Docs");
	my_chdir("home");
	int q = my_open("resume.txt");
	my_write(q,buf,strlen(buf));
	my_close(q);
	my_chdir("/");
	my_rmdir("home");

	if(my_chdir("home")<0){
		printf("unable to change directory to home\n");
	}

	if(my_open("/home/resume.txt")<0){
		printf("unable to open file\n");
	}

	if(my_chdir("/home/Docs")<0){
		printf("unable to change directory to /home/Docs\n");
	}

	return 0;
}