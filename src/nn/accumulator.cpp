#include "accumulator.hpp"
#include "common.hpp"
#include "../position.hpp"
#include "evaluate.hpp"

namespace Stella::Network {
    class Evaluator;
}

namespace Stella::Accumulator {

// This is used to reset the refresh table completely,
// loop through all entries leaving them blank
void RefreshTable::reset() {
    for (Color c : {WHITE, BLACK}) {
        for (size_t i = 0; i < 32; ++i) {
            std::memcpy(entries[c][i].values, Features::L0_BIAS, sizeof(int16_t) * Features::NB_L1);
            std::memset(entries[c][i].pieces, 0, sizeof(Bitboard) * PIECE_NB);
        }
    }
}

// Used to reset the accumulator using the pieces on the board
void AccumulatorTable::reset(Position* pos) {
    // Loop over both sides
    for (Color c : {WHITE, BLACK}) {
        // Get the king square
        const Square ksq = pos->ksq(c);
        // Reset the state
        int16_t* state = values[c];
        std::memcpy(state, Features::L0_BIAS, sizeof(int16_t) * Features::NB_L1);

        // Loop through the occupancy board and add to accumulator
        Bitboard occupied = pos->pieces();
        while (occupied) {
            Square sq = pop_lsb(occupied);
            Piece pc = pos->piece_on(sq);
            apply_delta<true>(state, make_index(sq, pc, ksq, c));
        }
        // Set the computed flag
        computed[c] = true;
    }
}

// Used to refresh the accumulator with the refresh table when required
void AccumulatorTable::refresh(Position* pos, Network::Evaluator* eval, Color side) {
    // Get info about the position
    const Square ksq = pos->ksq(side);
    const bool kingside = KING_SIDEBB & ksq;
    const uint16_t idx = (16 * kingside) + KingBuckets[side][ksq];

    // Get the refresh entry
    RefreshEntry* state = &eval->refreshTable->entries[side][idx];

    // Loop through the pieces for each side
    for (Color c : {WHITE, BLACK}) {
        for (PieceType pt = PAWN; pt <= KING; ++pt) {
            // Make the piece
            Piece pc = make_piece(c, pt);
            // Get the occupancy board for the piece
            Bitboard occupied = pos->pieces(pc);
            Bitboard pre = state->pieces[pc];

            // Find pieces to add and remove
            Bitboard remove = pre & ~occupied;
            Bitboard add = occupied & ~pre;

            // Loop through and add/remove all the pieces
            while (remove)
                apply_delta<false>(state->values, make_index(pop_lsb(remove), pc, ksq, side));
            while (add)
                apply_delta<true>(state->values, make_index(pop_lsb(add), pc, ksq, side));

            // Update the refresh entry
            state->pieces[pc] = occupied;
        }
    }

    // Copy over the state and set the computed flag
    std::memcpy(values[side], state->values, sizeof(int16_t) * Features::NB_L1);
    computed[side] = true;
}

}
