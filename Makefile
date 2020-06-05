CC = g++
CFLAGS = -std=c++11 -O3 -Wall -g -fPIC -Iinclude/
LDFLAGS = -lasound -lm -lpthread

SOURCES = $(wildcard src/*.cpp)
OBJECTS = $(patsubst %.cpp, %.o, $(SOURCES))
INCLUDES = $(wildcard include/*.hpp)

LIBRARY = libsynth.so

all: lib

lib: $(OBJECTS)
	$(CC) -rdynamic -shared $(CFLAGS) -o lib/$(LIBRARY) $(OBJECTS) $(LDFLAGS)

$(OBJECTS): src/%.o : src/%.cpp
	$(CC) $(CFLAGS) -c $< -o $@ $(LDFLAGS)

install:
	cp lib/$(LIBRARY) /usr/lib/
	mkdir -p /usr/include/Synth/
	cp include/* /usr/include/Synth/

example:
	$(CC) examples/example.cpp -o synth -lsynth

clean:
	rm -rf lib/$(LIBRARY) src/*.o
