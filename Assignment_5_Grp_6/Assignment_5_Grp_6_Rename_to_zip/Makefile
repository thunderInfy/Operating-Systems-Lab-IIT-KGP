all: Master sched process MMU
	
Master: Master.cpp
	g++ -std=c++11 Master.cpp -o Master

sched: sched.cpp
	g++ -std=c++11 sched.cpp -o sched

process: process.cpp
	g++ -std=c++11 process.cpp -o process

MMU: MMU.cpp
	g++ -std=c++11 MMU.cpp -o MMU

output:
	cat file1.txt file2.txt file3.txt > result.txt

clean: 
	rm Master sched process MMU file1.txt file2.txt file3.txt