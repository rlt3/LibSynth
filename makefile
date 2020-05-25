all: sin

usual: build.cpp
	g++ -Wall -g -o build build.cpp -lasound -lm
min: min.cpp
	g++ -Wall -g -o min min.cpp -lasound -lm
sin: sin.cpp
	g++ -Wall -g -o sin sin.cpp -lasound -lm
full: full.c
	gcc -Wall -g -o full full.c -lasound -lm
dev: dev-audio.c
	gcc -Wall -g -o test dev-audio.c
