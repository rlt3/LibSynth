#ifndef SYNTH_OPTION_HPP
#define SYNTH_OPTION_HPP

#include <string>

/* Base class for Synth to hold super in containers */
class SynthValue {
public:
    SynthValue (std::string name, std::string description)
        : _name (name)
        , _description (description)
    { }

    std::string getName () const
    {
        return _name;
    }

    std::string getDescription () const
    {
        return _description;
    }

    virtual void adjust ()
    { }

    virtual bool isKnob () const
    {
        return false;
    }

    virtual bool isSwitch () const
    {
        return false;
    }

private:
    std::string _name;
    std::string _description;
};

/* For continuous changes */
class SynthKnob : public SynthValue {
public:
    SynthKnob (std::string name,
               std::string desc,
               double initial,
               double step,
               double min,
               double max)
        : SynthValue(name, desc)
        , _value (initial)
        , _min (min)
        , _max (max)
    {
        /*
         * TODO: needs to accept a lambda which calls the correct methods or
         * functions that update values. Also needs a minimum, maximum, and
         * step values.
         *
         * The knobs will 'hold' the current values of various parameters and
         * use them as arguments to any subclasses of the synth. This means I
         * don't need to worry with having any extra paramters to 'update'
         * when adjusting a knob. When adjusting the knob, it calls the correct
         * member functions ...
         */
    }

    /*
     * Expects a value within [-1.0, 1.0] which is just a percentage turn in
     * some direction, e.g. 0.2 is turning 20% right, -0.5 is turning 50% left.
     */
    void adjust (double amount)
    {
    }

    bool isKnob () const
    {
        return true;
    }

private:
    double _value;
    double _step;
    double _min;
    double _max;
};

/* For discrete changes */
class SynthSwitch : public SynthValue {
public:
    SynthSwitch (std::string name, std::string desc)
        : SynthValue(name, desc)
    { }
private:
};

#endif
