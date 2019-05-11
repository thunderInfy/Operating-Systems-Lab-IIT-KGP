all:
	g++ -std=c++11 -c fat.cpp
	ar -rcs libfat.a fat.o
	g++ -std=c++11 userProgram.cpp -L. -lfat -o u
clean:
	rm libfat.a fat.o u