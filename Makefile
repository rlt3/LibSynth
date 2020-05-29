all: AudioDevice.cpp AudioDevice.cpp Oscillator.cpp Synth.cpp MidiController.cpp
	g++ -std=c++11 -Wall -g -o synth Synth.cpp Oscillator.cpp MidiController.cpp AudioDevice.cpp -lasound -lm -lpthread
