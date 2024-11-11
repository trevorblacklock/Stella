#ifndef EVALUATE_H_INCLUDED
#define EVALUATE_H_INCLUDED

#include "types.hpp"
#include "bitboard.hpp"
#include "position.hpp"

namespace Stella {

// Class to evaluate a position and return a value in centi-pawns.
namespace Evaluate {

// Store the current gamephase score
extern Value phase;
// Store sides to move
extern Color us;
extern Color them;

// Initializer for any tables used in evaluation
void init();

// Helper to get scores based on side to move
PhaseScore val(Color c, PhaseScore v);

// Sub functions or specific & complicated types of evaluation
template<PieceType pt>
PhaseScore evaluate_piece(Position* pos);

// Main call to evaluate a position
Value evaluate(Position* pos);
};

}

#endif
