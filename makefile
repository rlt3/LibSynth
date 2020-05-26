all: AudioDevice.cpp AudioDevice.cpp Synth.cpp
	g++ -Wall -g -o synth Synth.cpp AudioDevice.cpp -lasound -lm
