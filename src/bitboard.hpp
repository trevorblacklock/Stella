#ifndef BITBOARD_H_INCLUDED
#define BITBOARD_H_INCLUDED

#include <string>
#include <iostream>

#include "types.hpp"

namespace Stella {

constexpr Bitboard ALL_SQUARES = ~0;
constexpr Bitboard DARK_SQUARES = 0xAA55AA55AA55AA55ULL;

constexpr Bitboard FILE_ABB = 0x0101010101010101ULL;
constexpr Bitboard FILE_BBB = FILE_ABB << 1;
constexpr Bitboard FILE_CBB = FILE_ABB << 2;
constexpr Bitboard FILE_DBB = FILE_ABB << 3;
constexpr Bitboard FILE_EBB = FILE_ABB << 4;
constexpr Bitboard FILE_FBB = FILE_ABB << 5;
constexpr Bitboard FILE_GBB = FILE_ABB << 6;
constexpr Bitboard FILE_HBB = FILE_ABB << 7;

constexpr Bitboard RANK_1BB = 0xFF;
constexpr Bitboard RANK_2BB = RANK_1BB << 8;
constexpr Bitboard RANK_3BB = RANK_1BB << 16;
constexpr Bitboard RANK_4BB = RANK_1BB << 24;
constexpr Bitboard RANK_5BB = RANK_1BB << 32;
constexpr Bitboard RANK_6BB = RANK_1BB << 40;
constexpr Bitboard RANK_7BB = RANK_1BB << 48;
constexpr Bitboard RANK_8BB = RANK_1BB << 56;

constexpr Bitboard NOT_FILE_ABB = ~FILE_ABB;
constexpr Bitboard NOT_FILE_HBB = ~FILE_HBB;
constexpr Bitboard NOT_RANK_1BB = ~RANK_1BB;
constexpr Bitboard NOT_RANK_8BB = ~RANK_8BB;

constexpr Bitboard OUTER_SQUARESBB = FILE_ABB | FILE_BBB | RANK_1BB | RANK_8BB;

constexpr Bitboard QUEEN_SIDEBB = FILE_ABB | FILE_BBB | FILE_CBB | FILE_DBB;
constexpr Bitboard KING_SIDEBB = FILE_EBB | FILE_FBB | FILE_GBB | FILE_HBB;
constexpr Bitboard CENTER_FILESBB = FILE_CBB | FILE_DBB | FILE_EBB | FILE_FBB;
constexpr Bitboard CENTERBB = (FILE_DBB | FILE_EBB) & (RANK_4BB | RANK_5BB);

extern uint8_t SquareDistance[SQ_NB][SQ_NB];
extern Bitboard BetweenBB[SQ_NB][SQ_NB];
extern Bitboard LineBB[SQ_NB][SQ_NB];
extern Bitboard PseudoAttacks[PIECE_TYPE_NB][SQ_NB];
extern Bitboard PawnAttacks[COLOR_NB][SQ_NB];

// Function for returning a bitboard of a given square
constexpr Bitboard square_bb(Square s) {
    assert(is_ok(s));
    return (1ULL << s);
}

// Namespace Bitboard
namespace Bitboards {
// Print a bitboard in a 8x8 board representation
std::string print(Bitboard b);
// Initialize all bitboard arrays
void init();
}

// Overload the bitwise operators used between a Bitboard and Square type
inline Bitboard operator&(Bitboard b, Square s) { return b & square_bb(s); }
inline Bitboard operator|(Bitboard b, Square s) { return b | square_bb(s); }
inline Bitboard operator^(Bitboard b, Square s) { return b ^ square_bb(s); }
inline Bitboard operator&(Square s1, Square s2) { return square_bb(s1) & square_bb(s2); }
inline Bitboard operator|(Square s1, Square s2) { return square_bb(s1) | square_bb(s2); }
inline Bitboard& operator|=(Bitboard& b, Square s) { return b |= square_bb(s); }
inline Bitboard& operator^=(Bitboard& b, Square s) { return b ^= square_bb(s); }

// Returns a filled rank bitboard given a square
constexpr Bitboard rank_bb(Square s) {
    assert(is_ok(s));
    return RANK_1BB << (8 * rank_of(s));
}

// Returns a filled file bitboard given a square
constexpr Bitboard file_bb(Square s) {
    assert(is_ok(s));
    return FILE_ABB << file_of(s);
}

// Returns a bitboard of a line connecting square 1 and square 2
// which runs from board edge to edge and returns 0 if the squares
// cannot be connected via file/rank/diagonal.
inline Bitboard line_bb(Square s1, Square s2) {
    assert(is_ok(s1) && is_ok(s2));
    return LineBB[s1][s2];
}

// Returns a bitboard with spaces between square 1 and square 2 filled
// excluding square 1 but inclusive of square 2 and returns square 2 if
// the squares cannot be connected via file/rank/diagonal.
inline Bitboard between_bb(Square s1, Square s2) {
    assert(is_ok(s1) && is_ok(s2));
    return BetweenBB[s1][s2];
}

// Return true if square 3 lies along the line connecting square 1 and square 2
inline bool lies_along(Square s1, Square s2, Square s3) {
    return line_bb(s1, s2) & s3;
}

// Function that shifts the bitboard by a cardinal direction
constexpr Bitboard shift(Bitboard b, Direction d) {
    return d == NORTH      ? b << NORTH                     : d == NORTH_NORTH ? b << NORTH_NORTH
        : d == SOUTH      ? b >> -SOUTH                    : d == SOUTH_SOUTH ? b >> -SOUTH_SOUTH
        : d == EAST       ? (b & ~FILE_HBB) << EAST        : d == WEST        ? (b & ~FILE_ABB) >> -WEST
        : d == NORTH_EAST ? (b & ~FILE_HBB) << NORTH_EAST  : d == NORTH_WEST  ? (b & ~FILE_ABB) << NORTH_WEST
        : d == SOUTH_EAST ? (b & ~FILE_HBB) >> -SOUTH_EAST : d == SOUTH_WEST  ? (b & ~FILE_ABB) >> -SOUTH_WEST
        : 0;
}

// Return a bitboard of pawn attacks given the square and side
inline Bitboard pawn_attacks(Color c, Square s) {
    assert(is_ok(s));
    return PawnAttacks[c][s];
}

// Return a bitboard of pawn attacks given the assumed bitboard of pawns
inline Bitboard pawn_attacks_bb(Color c, Bitboard b) {
    if (!b) return 0;
    return c == WHITE ? shift(b, NORTH_WEST) | shift(b, NORTH_EAST)
                    : shift(b, SOUTH_WEST) | shift(b, SOUTH_EAST);
}

// Return rank distance between two squares
inline int rank_distance(Square s1, Square s2) {
    assert(is_ok(s1) && is_ok(s2));
    return std::abs(rank_of(s1) - rank_of(s2));
}

// Return file distance between two squares
inline int file_distance(Square s1, Square s2) {
    assert(is_ok(s1) && is_ok(s2));
    return std::abs(file_of(s1) - file_of(s2));
}

// Return the distance between two squares
inline int distance(Square s1, Square s2) {
    assert(is_ok(s1) && is_ok(s2));
    return SquareDistance[s1][s2];
}

// Struct for containing all information regarding magic bitboards.
// https://www.chessprogramming.org/Magic_Bitboards was used as a reference.
struct Magic {
public:
    Bitboard* attacks;
    Bitboard  mask;
    Bitboard  magic;
    uint32_t  shift;
    // Retrieve the attack bitboard's index
    uint32_t index(Bitboard occupied) const {
        // If Pext is available use it for a speedup
        if (hasPext) return uint32_t(pext(occupied, mask));
        // Otherwise use the standard approach
        return uint32_t(((occupied & mask) * magic) >> shift);
    }
};

// Store all possible slider piece attack bitboards.
// Table sizes are found by summing the total relevant occupancies per square,
// notice the bishop table is much smaller since it can attack less squares.
extern Bitboard RookAttacks[102400];
extern Bitboard BishopAttacks[5248];

// Store magic numbers for magic bitboard approach
extern Magic RookMagics[SQ_NB];
extern Magic BishopMagics[SQ_NB];

// Return the attacks for a given piece, square and occupied bitboard
inline Bitboard attacks_bb(Square s, PieceType pt, Bitboard occupied) {
    assert(pt != PAWN && is_ok(s));

    switch (pt) {
        case BISHOP: return BishopMagics[s].attacks[BishopMagics[s].index(occupied)];
        case ROOK: return RookMagics[s].attacks[RookMagics[s].index(occupied)];
        case QUEEN: return attacks_bb(s, ROOK, occupied) | attacks_bb(s, BISHOP, occupied);
        default: return PseudoAttacks[pt][s];
    }
}

// Return the attacks for a given piece, square and assumed zero occupied bitboard
inline Bitboard attacks_bb(Square s, PieceType pt) {
    assert(pt != PAWN && is_ok(s));
    return PseudoAttacks[pt][s];
}


// Count the number of bits in a bitboard
inline int popcount(Bitboard b) {
    #if defined(_WIN32) || defined(WIN32)
    return int(_mm_popcnt_u64(b));
    #else
    return __builtin_popcountll(b);
    #endif
}

// Return the least significant bit in a bitboard
inline Square lsb(Bitboard b) {
    assert(b);
    #if defined(_WIN32) || defined(WIN32)
    unsigned long idx;
    _BitScanForward64(&idx, b);
    return Square(idx);
    #else
    return Square(__builtin_ctzll(b));
    #endif
}

// Return the most significant bit in a bitboard
inline Square msb(Bitboard b) {
    assert(b);
    #if defined(_WIN32) || defined(WIN32)
    unsigned long idx;
    _BitScanReverse64(&idx, b);
    return Square(idx);
    #else
    return Square(__builtin_clzll(b));
    #endif
}

// Find the least significant bit and remove it from the bitboard
inline Square pop_lsb(Bitboard& b) {
    assert(b);
    const Square s = lsb(b);
    b &= b - 1;
    return s;
}

}

#endif
