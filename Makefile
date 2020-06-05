all: AudioDevice.cpp AudioDevice.cpp Oscillator.cpp Polyphonic.cpp Envelope.cpp Filter.cpp Synth.cpp MidiController.cpp
	g++ -std=c++11 -O3 -Wall -g -o synth Synth.cpp Polyphonic.cpp Envelope.cpp Filter.cpp Oscillator.cpp MidiController.cpp AudioDevice.cpp -lasound -lm -lpthread
