
#include <algorithm>

#include "history.hpp"

namespace Stella {

void History::clear() {
    killers.fill(Move::none());
    butterflyHistory.fill(VALUE_ZERO);
    continuationHistory.fill(VALUE_ZERO);
    captureHistory.fill(VALUE_ZERO);
    evalHistory.fill(VALUE_ZERO);
}

void History::clear_killers_grandchildren(Color side, int ply) {
    assert(ply < MAX_PLY + 2);
    killers[side][ply][0] = Move::none();
    killers[side][ply][1] = Move::none();
}

Value History::get_history(Position* pos, Move m, int ply) const {
    assert(m.is_ok());

    Color us     = pos->side();
    Color them   = ~us;
    Square from  = m.from();
    Square to    = m.to();
    Piece pc     = pos->piece_on(from);
    PieceType pt = piece_type(pc);

    // Check for a capture to use capture history
    if (pos->is_capture(m)) {
        Piece cap = pos->piece_on(to);
        return 10 * piece_value(cap).mid + get_capture(pc, to, piece_type(cap));
    }

    // Handle quiet moves
    else {
        Bitboard threatByPawn = pos->attacks_by(PAWN, them);
        Bitboard threatByMinor = pos->attacks_by(KNIGHT, them)
                               | pos->attacks_by(BISHOP, them)
                               | threatByPawn;
        Bitboard threatByRook = pos->attacks_by(ROOK, them)
                              | threatByMinor;

        Bitboard threatenedPieces = (pos->pieces(us, QUEEN) & threatByRook)
                                  | (pos->pieces(us, ROOK) & threatByMinor)
                                  | (pos->pieces(us, BISHOP, KNIGHT) & threatByPawn);

        Value v = get_butterfly(us, m) * 2
                + get_continuation(pc, to, ply - 1)
                + get_continuation(pc, to, ply - 2)
                + get_continuation(pc, to, ply - 3)
                + get_continuation(pc, to, ply - 4) / 4
                + get_continuation(pc, to, ply - 6);

        v += bool(pos->check_squares(pt) & to) * 16000;

        v += threatenedPieces & from ? (pt == QUEEN && !(threatByRook & to) ? 50000
                                     : pt == ROOK && !(threatByMinor & to)  ? 25000
                                     : !(threatByPawn & to)                 ? 15000 
                                     : 0) : 0;
        v -= !(threatenedPieces & from) ? (pt == QUEEN && (threatByRook & to)) ? 50000
                                        : (pt == ROOK && (threatByMinor & to)) ? 25000
                                        : 0 : 0;

        return v;
    }
}

Move History::get_killer(Color side, int ply, int id) const {
    assert(ply >= 0 && ply < MAX_PLY + 2);
    assert(id == 0 || id == 1);
    return killers[side][ply][id];
}

Value History::get_butterfly(Color side, Move m) const {
    assert(m.is_ok());
    return butterflyHistory[side][m.from()][m.to()];
}

Value History::get_capture(Piece pc, Square to, PieceType pt) const {
    return captureHistory[pc][to][pt];
}

Value History::get_continuation(Piece pc, Square sq, int ply) const {
    assert(ply >= -7 && ply < MAX_PLY);
    return continuationHistory[pc][sq][ply + 7];
}

Value History::get_eval(Color side, int ply) const {
    assert(ply >= 0 && ply < MAX_PLY);
    return evalHistory[side][ply];
}

void History::set_killer(Color side, Move m, int ply) {
    assert(ply >= 0 && ply < MAX_PLY + 2);
    if (killers[side][ply][0] != m) {
        killers[side][ply][1]  = killers[side][ply][0];
        killers[side][ply][0]  = m;
    }
}

void History::set_butterfly(Color side, Move m, Value v) {
    assert(m.is_ok());
    butterflyHistory[side][m.from()][m.to()] = v;
}

void History::set_capture(Piece pc, Square to, PieceType pt, Value v) {
    captureHistory[pc][to][pt] = v;
}

void History::set_continuation(Piece pc, Square sq, int ply, Value v) {
    assert(ply >= -7 && ply < MAX_PLY);
    continuationHistory[pc][sq][ply + 7] = v;
}

void History::set_eval(Color side, int ply, Value v) {
    assert(ply >= 0 && ply < MAX_PLY);
    evalHistory[side][ply] = v;
}

bool History::is_improving(Color side, int ply, Value v) const {
    assert(ply >= 0 && ply < MAX_PLY);
    if (ply >= 2) return v > evalHistory[side][ply - 2];
    return false;
}

bool History::is_killer(Color side, Move m, int ply) const {
    assert(ply >= 0 && ply < MAX_PLY + 2);
    assert(m.is_ok());
    return (m == killers[side][ply][0] || m == killers[side][ply][1]);
}

void History::update_butterfly(Color side, Move m, Value b) {
    assert(m.is_ok());
    auto& entry = butterflyHistory[side][m.from()][m.to()];
    entry += b - entry * abs(b) / 7000;
    assert(abs(entry) <= 7000);
}

void History::update_capture(Piece pc, Square to, PieceType pt, Value b) {
    auto& entry = captureHistory[pc][to][pt];
    entry += b - entry * abs(b) / 10000;
    assert(abs(entry) <= 10000);
}

void History::update_continuation(Piece pc, Square sq, int ply, Value b) {
    assert(ply >= -7 && ply < MAX_PLY);
    auto& entry = continuationHistory[pc][sq][ply + 7];
    entry += b - entry * abs(b) / 25000;
    assert(abs(entry) <= 25000);
}

}