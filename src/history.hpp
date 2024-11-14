#ifndef HISTORY_H_INCLUDED
#define HISTORY_H_INCLUDED

#include "types.hpp"
#include "position.hpp"

namespace Stella {

struct History {
    int test;
};

const inline Value get_move_score(Position* pos, Move m) {
    assert(m != Move::none());
    assert(m.is_ok());

    // Don't support captures
    if (pos->is_capture(m)) return 0;

    // Get some information about the move and the position
    Value val;
    Bitboard threats, threatByPawn, threatByMinor, threatByRook;
    Color us = pos->side();
    Color them = ~us;
    Square from = m.from();
    Square to = m.to();
    Piece pc = pos->piece_moved(m);
    PieceType pt = piece_type(pc);

    assert(pc != NO_PIECE);

    // Initialize the threats
    threatByPawn = pos->attacks_by(PAWN, them);
    threatByMinor = pos->attacks_by(KNIGHT, them)
                  | pos->attacks_by(BISHOP, them)
                  | threatByPawn;
    threatByRook = pos->attacks_by(ROOK, them)
                 | threatByMinor;
    threats = (pos->pieces(us, QUEEN) & threatByRook)
            | (pos->pieces(us, ROOK) & threatByMinor)
            | ((pos->pieces(BISHOP, KNIGHT) & pos->pieces(us)) & threatByPawn);

    // Threat bonuses for landing on a check square
    val += (bool)(pos->check_squares(pt) & to) * 15000;

    // Bonus for escaping a capture
    val += threats & from ? ((pt == QUEEN && !(threatByRook & to)) ? 50000
                          : (pt == ROOK && !(threatByMinor & to)) ? 25000
                          : !(threatByPawn & to) ? 15000 : 0) : 0;

    // Bonus for moving into a threatened square
    val -= (pt == QUEEN ? (bool)(threatByRook & to) * 50000
            : pt == ROOK ? (bool)(threatByMinor & to) * 25000
                         : (bool)(threatByPawn & to) * 15000);

    return val;
}

}

#endif
