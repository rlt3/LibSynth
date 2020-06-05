#ifndef DEFINITIONS_HPP
#define DEFINITIONS_HPP

#define CHK(stmt, msg) if((stmt) < 0) {puts("ERROR: "#msg); exit(1);}

#define DEBUG false
#define PI    3.1415926535897932384626433832795029L
#define TWOPI 6.2831853071795864769252867665590058L

#include <algorithm>

inline double
clamp (const double v, const double min, const double max)
{
    return std::max(min, std::min(v, max));
}

#endif
