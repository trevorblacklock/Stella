#include "pv.hpp"

#include <iterator>

namespace Stella {

// Define the operators for each PV structure
Move PvLine::operator[](Depth d) {
  assert(d >= 0 && d < MAX_PLY);
  return moves[d];
}

PvLine Pv::operator[](Depth d) {
  assert(d >= 0 && d < MAX_PLY);
  return lines[d];
}

// Function used to clear all Pv Lines
void Pv::reset() {
  for (auto &line : lines) line.size = 0;
}

// Function to update the Pv at a given ply
void Pv::update(Move m, Depth d) {
  // Set the first move at this depth
  lines[d][0] = m;
  // Copy the next pv to this one
  std::copy(std::begin(lines[d + 1].moves),
            std::begin(lines[d + 1].moves) + lines[d + 1].size,
            std::begin(lines[d].moves));
  // Update the size of the current Pv Line
  lines[d].size = lines[d + 1].size + 1;
}

}
