#ifndef PV_H_INCLUDED
#define PV_H_INCLUDED

#include "types.hpp"

namespace Stella {

// Structure to store a list of moves referred to as a PV line
struct PvLine {
  // Line must contain a max amount of moves equal to max search depth
  Move moves[MAX_PLY];
  // Store the number of moves in this line
  uint8_t size = 0;

  // Operator to index the structure and return the move at that index
  Move& operator[](Depth d);
  Move operator[](Depth d) const;

  // Reset the length of the pv line
  void reset();
};

// Structure to store a list of PV lines for each depth of the search
struct Pv {
  // Number of lines must never exceed the max search depth
  PvLine lines[MAX_PLY];
  // Store the number of lines
  uint8_t size = 0;

  // Operator to index the structure and return the line at that index
  PvLine& operator[](Depth d);
  PvLine operator[](Depth d) const;

  // Reset all PV lines in the structure
  void reset();
  // Update the PV table with a move at a given depth
  void update(Move m, Depth d);
};

}

#endif
