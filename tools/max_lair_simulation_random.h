#ifndef MAX_LAIR_SIMULATION_RANDOM_H
#define MAX_LAIR_SIMULATION_RANDOM_H

#include <cstdint>

inline uint64_t mix(uint64_t value){
    value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9;
    value = (value ^ (value >> 27)) * 0x94d049bb133111eb;
    return value ^ (value >> 31);
}

inline double uniform(uint64_t seed, uint64_t route, uint64_t attempt, uint64_t channel){
    uint64_t bits = mix(seed ^ mix(route + 0x9e3779b97f4a7c15)
        ^ mix(attempt + 0x243f6a8885a308d3) ^ mix(channel));
    return (double)(bits >> 11) / 9007199254740992.0;
}

#endif
