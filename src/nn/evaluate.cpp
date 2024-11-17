#include "evaluate.hpp"
#include "accumulator.hpp"
#include "../position.hpp"
#include "common.hpp"
#include "layers.hpp"

#include <emmintrin.h>
#include <immintrin.h>
#include <stdlib.h>

namespace Stella::Network {

// Define a function for summing a 32 byte register
inline int32_t sum_register_32(vec_reg_32& rst) {
    #if defined(__AVX512F__)
    const __m256i reduced8 = _mm256_add_epi32(_mm512_castsi512_si256(rst), _mm512_extracti32x8_epi32(rst, 1));
    #elif defined(__AVX2__)
    const __m256i reduced8 = rst;
    #endif

    #if defined(__AVX512F__) || defined(__AVX2__)
    const __m128i reduced4 = _mm_add_epi32(_mm256_castsi256_si128(reduced8), _mm256_extractf128_si256(reduced8, 1));
    #else
    const __m128i reduced4 = rst;
    #endif

    __m128i vsum = _mm_add_epi32(reduced4, _mm_srli_si128(reduced4, 8));
    vsum = _mm_add_epi32(vsum, _mm_srli_si128(vsum, 4));
    int32_t sum = _mm_cvtsi128_si32(vsum);
    return sum;
}

// Default constructor for evaluate
Evaluator::Evaluator() {
    refreshTable = std::make_unique<Accumulator::RefreshTable>(Accumulator::RefreshTable{});
    refreshTable->reset();
    history.reserve(MAX_PLY + 1);
    history.push_back(Accumulator::AccumulatorTable{});
    historyIdx = 0;
}

// Copier for evaluate objects
Evaluator::Evaluator(const Evaluator& e) {
    history = e.history;
    historyIdx = e.historyIdx;
}

// Assignment for evaluate objects
Evaluator& Evaluator::operator=(const Evaluator& e) {
    history = e.history;
    historyIdx = e.historyIdx;
    return *this;
}

// Reset the evaluator
void Evaluator::reset(Position* pos) {
    history.clear();
    historyIdx = 0;
    history[historyIdx].reset(pos);
}

// Reset the history only
void Evaluator::reset_history() {
    history.clear();
    history.push_back(Accumulator::AccumulatorTable{});
    historyIdx = 0;
}

template<bool undo>
void Evaluator::update_history(Position* pos, Move m, Piece pc, Piece cap) {
    // If the move is an undo then pop back the history
    if (undo) {
        history[historyIdx].computed[WHITE] = false;
        history[historyIdx].computed[BLACK] = false;
        if (historyIdx > 0) historyIdx--;
        return;
    }

    // Get the color of the piece
    const Color side = piece_color(pc);

    // When not a history can increment forward and allocate a new entry
    historyIdx++;
    if (historyIdx >= history.size()) history.resize(historyIdx + 3);

    // Loop through both sides
    for (Color c : {WHITE, BLACK}) {
        // Check if the move invokes a refresh
        if (c == side && Accumulator::is_refresh_required(pc, m.from(), m.to()))
            history[historyIdx].refresh(pos, this, c);
        // Now check if last index is computed, if so we can lazily update from there
        else if (history[historyIdx - 1].computed[c])
            apply_lazy_updates(pos, c, m, pc, cap);
        // Fallback onto a refresh in some cases
        else
            history[historyIdx].refresh(pos, this, c);
    }
}

template void Evaluator::update_history<true>(Position* pos, Move m, Piece pc, Piece cap);
template void Evaluator::update_history<false>(Position* pos, Move m, Piece pc, Piece cap);

void Evaluator::apply_lazy_updates(Position* pos, Color side, Move m, Piece pc, Piece cap) {
    // Get some info about the position
    const Square ksq = pos->ksq(side);
    const Color c = piece_color(pc);
    uint32_t idx = historyIdx;

    // Get info about the move
    Square from = m.from();
    Square to = m.to();

    // Get the to piece in the case of a promotion can be tricky
    Piece pcTo = pos->is_promotion(m) ? make_piece(c, m.promotion()) : pc;

    // Special case for castling, note captured piece is encoded as the rook here
    if (m.type() == CASTLING) {
        // Find out what type of castling is occuring and adjust the to square for the king
        Square kingFrom = from;
        Square kingTo = to > from ? relative_square(c, G1) : relative_square(c, C1);
        // Now find the square for the rook
        Square rookFrom = to;
        Square rookTo = to > from ? relative_square(c, F1) : relative_square(c, D1);
        // Get all the indices
        uint16_t kingFromIdx = Accumulator::make_index(kingFrom, pc, ksq, side);
        uint16_t rookFromIdx = Accumulator::make_index(rookFrom, cap, ksq, side);
        uint16_t kingToIdx = Accumulator::make_index(kingTo, pc, ksq, side);
        uint16_t rookToIdx = Accumulator::make_index(rookTo, cap, ksq, side);
        // Apply the transformation to the accumulator
        Accumulator::ssaa(&history[idx - 1], &history[idx], side, kingFromIdx, rookFromIdx, kingToIdx, rookToIdx);
    }
    // Special case for all captures
    else if (cap != NO_PIECE) {
        // Adjust capture square for enpassant moves
        Square capSq = (m.type() == EN_PASSANT) ? to - pawn_push(c) : to;
        // Get the index
        uint16_t fromIdx = Accumulator::make_index(from, pc, ksq, side);
        uint16_t toIdx = Accumulator::make_index(to, pcTo, ksq, side);
        uint16_t capIdx = Accumulator::make_index(capSq, cap, ksq, side);
        // Apply the transformation to the accumulator
        Accumulator::ssa(&history[idx - 1], &history[idx], side, fromIdx, capIdx, toIdx);
    }
    // Final case for normal moves
    else {
        // Get the index
        uint16_t fromIdx = Accumulator::make_index(from, pc, ksq, side);
        uint16_t toIdx = Accumulator::make_index(to, pcTo, ksq, side);
        // Apply the transformation to the accumulator
        Accumulator::sa(&history[idx - 1], &history[idx], side, fromIdx, toIdx);
    }
}

Value Evaluator::propagate(Color side) {
    // Init registers for relu
    constexpr vec_reg_16 relu{};

    // Get the accumulator for each relative side
    const auto us = (vec_reg_16*) &history[historyIdx].values[side];
    const auto them = (vec_reg_16*) &history[historyIdx].values[~side];

    // Create a register for the result
    vec_reg_32 result{};

    // Get the weights
    const auto weight = (vec_reg_16*) Features::L1_WEIGHT;

    // Loop through our side
    for (int i = 0; i < Features::NB_L1 / INT16_SPACING; ++i) {
        result = vec_add_32(result, vec_madd_16(vec_max_16(us[i], relu), weight[i]));
    }
    // Loop through their side, applying the offset
    for (int i = 0; i < Features::NB_L1 / INT16_SPACING; ++i) {
        int weightIdx = i + Features::NB_L1 / INT16_SPACING;
        result = vec_add_32(result, vec_madd_16(vec_max_16(them[i], relu), weight[weightIdx]));
    }

    // Sum over all the registers
    const auto output = sum_register_32(result) + Features::L1_BIAS[0];
    // Return the scaled output
    return output / 32 / 128;
}

Value Evaluator::predict(Position* pos) {
    // Reset the accumulator using the position
    history[historyIdx].reset(pos);
    return propagate(pos->side());
}

}
