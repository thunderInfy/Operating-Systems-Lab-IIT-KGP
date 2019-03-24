all: Master sched process MMU
	
Master: Master.cpp
	g++ -std=c++11 Master.cpp -o Master

sched: sched.cpp
	g++ -std=c++11 sched.cpp -o sched

process: process.cpp
	g++ -std=c++11 process.cpp -o process

MMU: MMU.cpp
	g++ -std=c++11 MMU.cpp -o MMU

clean: 
	rm Master sched process MMU 