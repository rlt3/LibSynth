# LibSynth

A simple drop-in MIDI synthesizer library for Linux.

Ever needed to programmatically play notes? Bought a MIDI keyboard to
create fun sounds but don't really need
a [DAW](https://en.wikipedia.org/wiki/Digital_audio_workstation)? This is the
library for you: start hearing sound in two lines of code.

## Requirements

This requires
[ALSA](https://en.wikipedia.org/wiki/Advanced_Linux_Sound_Architecture), thus it
only works on Linux systems.  Debian-based systems like Ubuntu can install the
ALSA bindings with the following command:

    sudo apt install libasound2-dev

# Installation

This library is built using the standard `make && make install` loop.

To compile the library:

    make

To install the library:

    sudo make install

To make the example synthesizer and hear some playback immediately:

    make example
    ./synth -p acid

# How do I use this to play notes or make sounds?

Right now there is no real documentation, but the interface is meant to be
dead-simple. I recommend looking at the example program in the examples
directory which can be built using `make example`.

Simply include `<Synth/Synth.hpp>`, link with `-lsynth` and make a `Synth`
object.

Each Synth object spawns its own audio thread in the background. To play a note
call the `noteOn` method which accepts an [integer
note](https://www.inspiredacoustics.com/en/MIDI_note_numbers_and_center_frequencies)
and a velocity, representing how 'hard' the note was played, e.g. how hard the
key was pressed on a keyboard or hit on a pad -- `1.0` being hardest, `0.0`
being softest.

Calling `noteOn` is effectively the same as pressing and holding down a key on
a keyboard. You need to call 'noteOff' to release the note.

To make sounds this is all you need to know, but there are more knobs to adjust:
It features the standard [ADSR
envelope](https://en.wikipedia.org/w/index.php?title=ADSR_envelope&redirect=yes)
along with a low pass filter and an ADSR envelope for that filter.

# I have a MIDI keyboard, how do I use it? 

Once you've installed ALSA and it's utilities (see above), make sure your
keyboard is plugged in. Then call `aconnect -l` from the command line to list
currently connected devices. Here's some example output from my terminal:

    client 14: 'Midi Through' [type=kernel]
        0 'Midi Through Port-0'
    client 24: 'MPKmini2' [type=kernel,card=2]
        0 'MPKmini2 MIDI 1 '
    client 28: 'Virtual Raw MIDI 3-0' [type=kernel,card=3]
        0 'VirMIDI 3-0     '
    client 29: 'Virtual Raw MIDI 3-1' [type=kernel,card=3]

My keyboard is the `MPKmini2` keyboard so I provide that string name to the
`Synth` class on instantiation, e.g. `Synth synth("MPKmini2")`. And that's it.
You should be able to play notes on your physical keyboard and hear it on your
speakers.

Using the example program, I can run `./synth -d MPKmini2` to play notes with
my keyboard as well.

# Limitations & TODO

Although the scope of this synthesizer is meant to be small and not replace
a DAW in any capacity, there's still many things that would make this better
such as:

    * A low-frequency oscillator
    * A simple software arpeggiator 
    * Simple drum machine (a special low note arpeggiator, I guess...)
    * Support more than 2 channels
    * Support other sample rates -- only supports 44100Hz right now (CD Quality)
    * Documentation

# Shout Outs

This was a fun project. I wanted to make it because there's simply no drop-in
libraries that I could find where I could just play notes programmatically while
also using my keyboard. I just wanted to make beep boop.

Special thanks to the tutorials of [Martin
Finke](http://www.martin-finke.de/blog/tags/making_audio_plugins.html) and the
book BasicSynth by Daniel Mitchell.
