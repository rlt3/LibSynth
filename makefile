all: build.cpp
	g++ -Wall -g -o build build.cpp -lasound -lm
