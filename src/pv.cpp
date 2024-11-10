#include "pv.hpp"

#include <iterator>
#include <cstring>

namespace Stella {

// Define the operators for each PV structure
Move& PvLine::operator[](Depth d) {
    assert(d >= 0 && d < MAX_PLY);
    return moves[d];
}

Move PvLine::operator[](Depth d) const {
    assert(d >= 0 && d < MAX_PLY);
    return moves[d];
}

PvLine& Pv::operator[](Depth d) {
    assert(d >= 0 && d < MAX_PLY);
    return lines[d];
}

PvLine Pv::operator[](Depth d) const {
    assert(d >= 0 && d < MAX_PLY);
    return lines[d];
}

// Function used to clear all Pv Lines
void Pv::reset() {
    for (auto &line : lines) line.size = 0;
}

void PvLine::reset() {
    size = 0;
}

// Function to update the Pv at a given ply
void Pv::update(Move m, Depth d) {
    // Set the first move at this depth
    lines[d][0] = m;
    // Copy the next pv to this one
    std::memcpy(&lines[d][1],
                &lines[d + 1][0],
                sizeof(Move) * lines[d + 1].size);
    // Update the size of the current Pv Line
    lines[d].size = lines[d + 1].size + 1;
}

}
