#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <csignal>
#include <unistd.h>
#include <Synth/Synth.hpp>

static bool progRunning = true;

/*
 * Catching Ctrl+C allows for all the devices to clean up safely via their
 * destructor.
 */
void
sigint (int sig_num) 
{ 
    progRunning = false;
    printf("\nExiting ...\n");
    fflush(stdout);
}

typedef void (*Preset) (Synth &synth);

void
preset_default (Synth &synth)
{
    synth.setAttack(0.01);
    synth.setDecay(0.5);
    synth.setSustain(1.0);
    synth.setRelease(1.0);

    synth.setFilterAttack(0.01);
    synth.setFilterDecay(0.5);
    synth.setFilterSustain(1.0);
    synth.setFilterRelease(1.0);

    synth.setCutoff(0.99);
    synth.setResonance(0.0);
}

void
preset_acid (Synth &synth)
{
    synth.setAttack(0.01);
    synth.setDecay(0.4);
    synth.setSustain(0.5);
    synth.setRelease(0.4);

    synth.setFilterAttack(0.35);
    synth.setFilterDecay(0.40);
    synth.setFilterSustain(0.01);
    synth.setFilterRelease(0.01);

    synth.setCutoff(0.15);
    synth.setResonance(0.90);
}

void
preset_pluck (Synth &synth)
{
    synth.setAttack(0.01);
    synth.setDecay(1.5);
    synth.setSustain(0.01);
    synth.setRelease(1.5);

    synth.setFilterAttack(0.5);
    synth.setFilterDecay(1.5);
    synth.setFilterSustain(0.01);
    synth.setFilterRelease(1.5);

    synth.setCutoff(0.85);
    synth.setResonance(0.85);
}

void
usage (int argc, char **argv)
{
    fprintf(stderr,
            "Usage: %s [-h] [-p <preset>] [-d <midi device>]\n"
            "   -p <preset>\n"
            "       Use one of the presets: default, acid, pluck\n"
            "   -d <midi device>\n"
            "      Connect to a midi device. Expects a string name\n"
            "      from `aconnect -o`\n"
            "   -h\n"
            "      Display this help menu and exit.\n"
            , argv[0]);
    exit(1);
}

Preset
handle_args (int argc, char **argv, const char **device)
{
    Preset preset = preset_default;

    if (argc == 1)
        return preset;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0) {
            usage(argc, argv);
        }
        else if (strcmp(argv[i], "-p") == 0) {
            if (++i >= argc)
                usage(argc, argv);
            if (strcmp(argv[i], "default") == 0)
                preset = preset_default;
            else if (strcmp(argv[i], "acid") == 0)
                preset = preset_acid;
            else if (strcmp(argv[i], "pluck") == 0)
                preset = preset_pluck;
            else
                usage(argc, argv);
        }
        else if (strcmp(argv[i], "-d") == 0) {
            if (++i >= argc)
                usage(argc, argv);
            *device = argv[i];
        }
    }

    return preset;
}

int
main (int argc, char **argv)
{
    signal(SIGINT, sigint);

    const char *midiDevice = NULL;

    Preset preset = handle_args(argc, argv, &midiDevice);
    Synth synth(midiDevice);
    synth.setVolume(0.8);
    preset(synth);

    /* how hard the note is played (how loud it will be) in range [0.0, 1.0] */
    const double velocity = 1.0;
    /* a low note */
    const int note = 32;

    bool noteon = true;
    while (progRunning) {
        /* play a note every second, turning it on and off every half second */
        if (noteon)
            synth.noteOn(note, velocity);
        else
            synth.noteOff(note);
        noteon = !noteon;
        usleep(500000);
    }

    return 0;
}
