all:
	g++ -std=c++11 -c myfs.cpp
	ar -rcs libmyfs.a myfs.o
	g++ -std=c++11 userProgram.cpp -L. -lmyfs -o u
clean:
	rm libmyfs.a myfs.o u