#ifndef ACCUMULATOR_H_INCLUDED
#define ACCUMULATOR_H_INCLUDED

#include "../types.hpp"
#include "common.hpp"
#include "layers.hpp"

namespace Stella {

static constexpr int KingBuckets[COLOR_NB][SQ_NB] {
{
    0,  1,  2,  3,  3,  2,  1,  0,
    4,  5,  6,  7,  7,  6,  5,  4,
    8,  9,  10, 11, 11, 10, 9,  8,
    8,  9,  10, 11, 11, 10, 9,  8,
    12, 12, 13, 13, 13, 13, 12, 12,
    12, 12, 13, 13, 13, 13, 12, 12,
    14, 14, 15, 15, 15, 15, 14, 14,
    14, 14, 15, 15, 15, 15, 14, 14
},
{
    14, 14, 15, 15, 15, 15, 14, 14,
    14, 14, 15, 15, 15, 15, 14, 14,
    12, 12, 13, 13, 13, 13, 12, 12,
    12, 12, 13, 13, 13, 13, 12, 12,
    8,  9,  10, 11, 11, 10, 9,  8,
    8,  9,  10, 11, 11, 10, 9,  8,
    4,  5,  6,  7,  7,  6,  5,  4,
    0,  1,  2,  3,  3,  2,  1,  0
}
};

static constexpr Square OrientSq[COLOR_NB][SQ_NB] = {
{
    A1, B1, C1, D1, E1, F1, G1, H1,
    A2, B2, C2, D2, E2, F2, G2, H2,
    A3, B3, C3, D3, E3, F3, G3, H3,
    A4, B4, C4, D4, E4, F4, G4, H4,
    A5, B5, C5, D5, E5, F5, G5, H5,
    A6, B6, C6, D6, E6, F6, G6, H6,
    A7, B7, C7, D7, E7, F7, G7, H7,
    A8, B8, C8, D8, E8, F8, G8, H8
},
{
    A8, B8, C8, D8, E8, F8, G8, H8,
    A7, B7, C7, D7, E7, F7, G7, H7,
    A6, B6, C6, D6, E6, F6, G6, H6,
    A5, B5, C5, D5, E5, F5, G5, H5,
    A4, B4, C4, D4, E4, F4, G4, H4,
    A3, B3, C3, D3, E3, F3, G3, H3,
    A2, B2, C2, D2, E2, F2, G2, H2,
    A1, B1, C1, D1, E1, F1, G1, H1
}
};

namespace Accumulator {

// Structure for an entry in the refresh table
struct RefreshEntry {
    alignas(BYTE_ALIGNMENT) int16_t values[Features::NB_L1]{};
    Bitboard pieces[PIECE_NB];
};

// The refresh table used for updating the accumulator,
// stores an entry for each piece per board view.
struct RefreshTable {
    RefreshEntry entries[COLOR_NB][32]{};
    void reset();
};

// Structure for storing the accumulator which is continuosly
// updated as moves are made / pieces are placed on the board.
// This is all possible since the input of the network is so
// sparse and select weights can be turned on and off for piece
// movements.
struct AccumulatorTable {
    bool computed[COLOR_NB]{};
    alignas(BYTE_ALIGNMENT) int16_t values[COLOR_NB][Features::NB_L1]{};
    void reset(Position* pos);
    void refresh(Position* pos, Network::Evaluator* eval, Color side);
};

// Return an index relating to a feature of the network
inline uint16_t make_index(Square sq, Piece pc, Square ksq, Color side) {
    // Get the square to move from the perspective of the side
    int s = OrientSq[side][sq];
    // Change the square if the ksq is on queenside vs kingside,
    // this is a little trick to differentiate between the two a small
    // amount since king buckets are presently mirrored.
    s ^= 7 * !!(ksq & 0x4);

    // Calculate the index
    uint16_t idx = s
        + (piece_type(pc) - 1) * SQ_NB
        + !(piece_color(pc) ^ side) * SQ_NB * 6
        + KingBuckets[side][ksq] * SQ_NB * 6 * 2;

    // Make sure the index is within bounds
    assert(idx < Features::NB_L0);
    return idx;
}

// Check if a given move triggers a refresh of the accumulator,
// this is usually just when a king is in a different bucket
inline bool is_refresh_required(Piece pc, Square from, Square to) {
    if (piece_type(pc) != KING) return false;
    // If crosses half then a refresh is required
    if (file_of(from) + file_of(to) == 7) return true;
    // If in a different king bucket then refresh
    return KingBuckets[piece_color(pc)][from] != KingBuckets[piece_color(pc)][to];
}

// Function to apply delta to a given accumulator entry
template<bool add>
inline void apply_delta(int16_t* source, int16_t* target, const uint16_t idx) {
    // Init the registers
    vec_reg_16 rst[NB_REGISTER];

    // Loop through each chunk
    for (size_t i = 0; i < Features::NB_L1 / CHUNK_UNROLL; ++i) {
        // Calculate an offset for this index
        const size_t offset = i * CHUNK_UNROLL;

        // Retrieve the weights, inputs and outputs
        const auto weight = (vec_reg_16*) &Features::L0_WEIGHT[idx * Features::NB_L1 + offset];
        const auto input = (vec_reg_16*) &source[offset];
        auto output = (vec_reg_16*) &target[offset];

        // Loop through registers and add/subtract the weights storing them in the output
        for (size_t x = 0; x < NB_REGISTER; ++x) {
            rst[x] = vec_load(&input[x]);
            rst[x] = add ? vec_add_16(rst[x], weight[x])
                         : vec_sub_16(rst[x], weight[x]);
            vec_store(&output[x], rst[x]);
        }
    }
}

template<bool add> inline void apply_delta(int16_t* source, const uint16_t index) {
    apply_delta<add>(source, source, index);
}

// Need three different functions for subtracting and adding indices for all kinds of moves.
// Possible moves are quiets which require a sub and an add, a capture which needs two subs
// and one add, then finally a castle which requires two subs and two adds.
inline void sa(AccumulatorTable* source, AccumulatorTable* target, Color side,
                uint16_t idx1, uint16_t idx2) {
    // Init the registers
    vec_reg_16 rst[NB_REGISTER];

    // Get the in and out for the accumulators
    const auto in = source->values[side];
    const auto out = target->values[side];

    // Loop through each chunk
    for (int i = 0; i < Features::NB_L1 / CHUNK_UNROLL; ++i) {
        // Calculate an offset for this index
        const size_t offset = i * CHUNK_UNROLL;

        // Retrieve the weights, inputs and outputs
        auto weight1 = (vec_reg_16*) &Features::L0_WEIGHT[idx1 * Features::NB_L1 + offset];
        auto weight2 = (vec_reg_16*) &Features::L0_WEIGHT[idx2 * Features::NB_L1 + offset];
        auto input = (vec_reg_16*) &in[offset];
        auto output = (vec_reg_16*) &out[offset];

        // Loop through registers and add/subtract the weights storing them in the output
        for (size_t x = 0; x < NB_REGISTER; ++x) {
            rst[x] = vec_load(&input[x]);
            rst[x] = vec_sub_16(rst[x], weight1[x]);
            rst[x] = vec_add_16(rst[x], weight2[x]);
            vec_store(&output[x], rst[x]);
        }
    }
}

inline void ssa(AccumulatorTable* source, AccumulatorTable* target, Color side,
                uint16_t idx1, uint16_t idx2, uint16_t idx3) {
    // Init the registers
    vec_reg_16 rst[NB_REGISTER];

    // Get the in and out for the accumulators
    const auto in = source->values[side];
    const auto out = target->values[side];

    // Loop through each chunk
    for (int i = 0; i < Features::NB_L1 / CHUNK_UNROLL; ++i) {
        // Calculate an offset for this index
        const size_t offset = i * CHUNK_UNROLL;

        // Retrieve the weights, inputs and outputs
        auto weight1 = (vec_reg_16*) &Features::L0_WEIGHT[idx1 * Features::NB_L1 + offset];
        auto weight2 = (vec_reg_16*) &Features::L0_WEIGHT[idx2 * Features::NB_L1 + offset];
        auto weight3 = (vec_reg_16*) &Features::L0_WEIGHT[idx3 * Features::NB_L1 + offset];
        auto input = (vec_reg_16*) &in[offset];
        auto output = (vec_reg_16*) &out[offset];

        // Loop through registers and add/subtract the weights storing them in the output
        for (size_t x = 0; x < NB_REGISTER; ++x) {
            rst[x] = vec_load(&input[x]);
            rst[x] = vec_sub_16(rst[x], weight1[x]);
            rst[x] = vec_sub_16(rst[x], weight2[x]);
            rst[x] = vec_add_16(rst[x], weight3[x]);
            vec_store(&output[x], rst[x]);
        }
    }
}

inline void ssaa(AccumulatorTable* source, AccumulatorTable* target, Color side,
                uint16_t idx1, uint16_t idx2, uint16_t idx3, uint16_t idx4) {
    // Init the registers
    vec_reg_16 rst[NB_REGISTER];

    // Get the in and out for the accumulators
    const auto in = source->values[side];
    const auto out = target->values[side];

    // Loop through each chunk
    for (int i = 0; i < Features::NB_L1 / CHUNK_UNROLL; ++i) {
        // Calculate an offset for this index
        const size_t offset = i * CHUNK_UNROLL;

        // Retrieve the weights, inputs and outputs
        auto weight1 = (vec_reg_16*) &Features::L0_WEIGHT[idx1 * Features::NB_L1 + offset];
        auto weight2 = (vec_reg_16*) &Features::L0_WEIGHT[idx2 * Features::NB_L1 + offset];
        auto weight3 = (vec_reg_16*) &Features::L0_WEIGHT[idx3 * Features::NB_L1 + offset];
        auto weight4 = (vec_reg_16*) &Features::L0_WEIGHT[idx4 * Features::NB_L1 + offset];
        auto input = (vec_reg_16*) &in[offset];
        auto output = (vec_reg_16*) &out[offset];

        // Loop through registers and add/subtract the weights storing them in the output
        for (size_t x = 0; x < NB_REGISTER; ++x) {
            rst[x] = vec_load(&input[x]);
            rst[x] = vec_sub_16(rst[x], weight1[x]);
            rst[x] = vec_sub_16(rst[x], weight2[x]);
            rst[x] = vec_add_16(rst[x], weight3[x]);
            rst[x] = vec_add_16(rst[x], weight4[x]);
            vec_store(&output[x], rst[x]);
        }
    }
}

}
}

#endif
