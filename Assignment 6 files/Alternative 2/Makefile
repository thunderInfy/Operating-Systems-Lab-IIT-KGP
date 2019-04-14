all:
	g++ -std=c++11 -c myfs_adi.cpp
	ar -rcs libmyfs.a myfs_adi.o
	g++ -std=c++11 userProgram.cpp -L. -lmyfs -o u
clean:
	rm libmyfs.a myfs_adi.o u