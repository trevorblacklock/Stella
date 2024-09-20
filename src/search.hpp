#ifndef SEARCH_H_INCLUDED
#define SEARCH_H_INCLUDED

#include "position.hpp"
#include "movegen.hpp"
#include "misc.hpp"
#include "timing.hpp"
#include "hashtable.hpp"
#include "history.hpp"
#include "pv.hpp"

namespace Stella {

// Structure for storing a "Root Move" which is a move that exists in the root position
struct RootMove {
  // Store the move
  Move m;

  // Create an operator to check if one Root Move is equal to another
  bool operator==(const Move& move) { return move == m; }

  // Store average, current and previous score found through search
  Value averageScore =  -VALUE_INFINITE;
  Value previousScore = -VALUE_INFINITE;
  Value currentScore =  -VALUE_INFINITE;

  // Constructor for a Root Move
  RootMove(Move move) { m = move; }
};

// Structure for storing information to be continuously updated and used through recursive search
struct SearchData {
  
};

}

#endif
