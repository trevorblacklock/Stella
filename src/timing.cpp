#include "types.hpp"
#include "timing.hpp"

#include <cmath>

namespace Stella {

TimeManager::TimeManager() {
    reset();
}

void TimeManager::reset() {
    timer.start();
    forceStop = false;
    depthLimit = {};
    nodeLimit = {};
    moveTimeLimit = {};
    timeLimit = {};
}

uint64_t TimeManager::elapsed() {
    timer.end();
    return timer.elapsed();
}

// Sets a limit to the depth that can be searched.
void TimeManager::set_depth_limit(Depth d) {
    depthLimit.enabled = true;
    depthLimit.max = d;
}

// Sets a limit to the number of nodes that can be searched.
void TimeManager::set_node_limit(uint64_t nodes) {
    nodeLimit.enabled = true;
    nodeLimit.max = nodes;
}

// Sets a time limit per move.
void TimeManager::set_move_time_limit(uint64_t time) {
    moveTimeLimit.enabled = true;
    moveTimeLimit.max = time;
}

// Given the total time, increment and expected number of moves left,
// calculates a maximum time to spend on the current move. This is
// further shortened using heuristics from the search to determine
// if that maximum times is even needed.
void TimeManager::set_time_limit(uint64_t time, uint64_t inc, int movesLeft, int ply) {
    // Set move overhead if no increment to account for delays.
    int overhead = inc ? 0 : 10;

    // Clamp the moves left to 50.
    int mtg = movesLeft ? std::min(movesLeft, 50) : 50;

    // If low time and low increment compress the moves to go
    if (time < 1000 && inc < 0.05) {
        mtg = time * 0.05;
    }

    // Make sure the time left is above zero.
    uint64_t timeLeft = std::max(static_cast<uint64_t>(1), time + inc * mtg 
                      - overhead * mtg);

    // Store the maximum scale.
    double optimalScale, maxScale;

    // If unknown number of moves left, extrapolate using log scale
    // to derive a maximum scalar to use on timeLeft.
    if (!movesLeft) {
        // Calculate time constants in log scale.
        double logTime = std::log10(timeLeft / 1000.0);
        double optimal = std::min(0.003 + 0.0005 * logTime, 0.005);
        double max = std::max(3.5 + 3.0 * logTime, 2.9);

        // Use time constants to find max  and optimal scale.
        optimalScale = std::min(0.01 + std::pow(ply, 0.5) * optimal, 0.2 * time / timeLeft);
        maxScale = std::min(6.0, max + ply / 10.0);
    }

    // If moves left are known, simply use that number of moves in timeLeft.
    else {
        optimalScale = std::min(ply / 500.0 + 0.5 / mtg, 0.9 * time / timeLeft);
        maxScale = std::min(6.0, 1.5 + 0.1 * mtg);
    }                 

    // Calculate max and optimal time to spend on the current move.
    uint64_t optimalTime = timeLeft * optimalScale;
    uint64_t maxTime = std::min(0.7 * time - overhead, maxScale * optimalTime);

    // Store these values.
    timeLimit.enabled = true;
    timeLimit.max = maxTime;
    timeLimit.optimal = optimalTime;
}

}