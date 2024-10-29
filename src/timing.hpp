#ifndef TIMING_HPP_INCLUDED
#define TIMING_HPP_INCLUDED

#include "types.hpp"
#include "misc.hpp"

namespace Stella {

struct Limits {
    bool enabled = false;
    uint64_t max;
};

struct TimeLimits : Limits {
    uint64_t optimal;
};

// Class to control time management while searching.
// Will use a given search limit to determine if the current
// search should be stopped.
class TimeManager {
public:
    // Constructor
    TimeManager();
    // Store all different types of limits
    Limits depthLimit;
    Limits nodeLimit;
    Limits moveTimeLimit;
    TimeLimits timeLimit;

    // Store move overhead in milliseconds
    int moveOverhead;

    // Flag for force stopping search
    bool forceStop;

    // Start time of search
    Timer timer;

    // Functions to set the limits
    void set_depth_limit(Depth d);
    void set_node_limit(uint64_t nodes);
    void set_move_time_limit(uint64_t time);
    void set_time_limit(uint64_t time, uint64_t inc, int movesLeft, int ply);

    // Set the start time
    void start_time();

    // Reset the time manager
    void reset();

    // Get the elapsed time in milliseconds
    uint64_t elapsed();

    // Set the move overhead
    void set_move_overhead(int time);

    // Stop the search
    void stop() { forceStop = true; };
};

}

#endif