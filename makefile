all: AudioDevice.cpp AudioDevice.cpp Oscillator.cpp Synth.cpp
	g++ -Wall -g -o synth Synth.cpp Oscillator.cpp AudioDevice.cpp -lasound -lm
