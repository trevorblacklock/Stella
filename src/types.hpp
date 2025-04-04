// This is a header file that contains all the data types used throughout Stella
// The file also includes os specific commands and common functions

#ifndef TYPES_H_INCLUDED
#define TYPES_H_INCLUDED

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdlib>

// When compiling using the makefile configure the program to run on WINDOWS or LINUX
// This is all done automatically and to improve performance/compatibility

#if defined(_WIN32) || defined(WIN32)
#include <intrin.h>
#include <nmmintrin.h>
#endif

#ifdef USE_PEXT
constexpr bool hasPext = true;
#include <immintrin.h>
#define pext(b, m) _pext_u64(b, m)
#else
constexpr bool hasPext = false;
#define pext(b, m) 0
#endif

namespace Stella {

using Bitboard = uint64_t;
using Key      = uint64_t;
using Value    = int;
using Depth    = int;

constexpr int MAX_MOVES = 256;
constexpr int MAX_PLY = 246;

enum Color {
    WHITE,
    BLACK,
    COLOR_NB = 2
};

enum GamePhase {
    MIDGAME,
    ENDGAME,

    GAME_PHASE_NB = 2,
    MIDGAME_CAP = 13500,
    ENDGAME_CAP = 4000
};

enum CastlingRights {
    NO_CASTLE,
    WHITE_KING,
    WHITE_QUEEN = WHITE_KING << 1,
    BLACK_KING = WHITE_KING << 2,
    BLACK_QUEEN = WHITE_KING << 3,

    KING_SIDE = WHITE_KING | BLACK_KING,
    QUEEN_SIDE = WHITE_QUEEN | BLACK_QUEEN,
    WHITE_CASTLE = WHITE_KING | WHITE_QUEEN,
    BLACK_CASTLE = BLACK_KING | BLACK_QUEEN,
    ANY_CASTLE = WHITE_CASTLE | BLACK_CASTLE,

    CASTLE_RIGHT_NB = 16
};

enum Bound {
    BOUND_NONE,
    BOUND_UPPER,
    BOUND_LOWER,
    BOUND_EXACT = BOUND_UPPER | BOUND_LOWER
};

enum NodeType {
    NON_PV,
    PV
};

enum Scoring {
    VALUE_ZERO = 0,
    VALUE_DRAW = 0,
    VALUE_NONE = 32002,
    VALUE_INFINITE = 32001,

    VALUE_MATE = 32000,
    VALUE_MATE_IN_MAX_PLY = VALUE_MATE - MAX_PLY,
    VALUE_MATED_IN_MAX_PLY = -VALUE_MATE_IN_MAX_PLY,
    VALUE_WIN = VALUE_MATE_IN_MAX_PLY - 1,
    VALUE_LOSS = -VALUE_WIN,
    VALUE_WIN_MAX_PLY = VALUE_WIN - MAX_PLY,
    VALUE_LOSS_MAX_PLY = VALUE_LOSS + MAX_PLY
};

enum PieceType {
    NO_PIECE_TYPE,
    PAWN,
    KNIGHT,
    BISHOP,
    ROOK,
    QUEEN,
    KING,
    ALL_PIECES = 0,
    PIECE_TYPE_NB = 8
};

enum Direction : int {
    NORTH = 8,
    EAST = 1,
    SOUTH = -NORTH,
    WEST = -EAST,

    NORTH_WEST = NORTH + WEST,
    NORTH_EAST = NORTH + EAST,
    SOUTH_WEST = -NORTH_EAST,
    SOUTH_EAST = -NORTH_WEST,

    NORTH_NORTH = NORTH + NORTH,
    SOUTH_SOUTH = -NORTH_NORTH
};

enum Piece {
    NO_PIECE,
    W_PAWN = PAWN, W_KNIGHT, W_BISHOP, W_ROOK, W_QUEEN, W_KING,
    B_PAWN = W_PAWN + 8, B_KNIGHT, B_BISHOP, B_ROOK, B_QUEEN, B_KING,
    PIECE_NB = 16
};

enum File : int {
    FILE_A,
    FILE_B,
    FILE_C,
    FILE_D,
    FILE_E,
    FILE_F,
    FILE_G,
    FILE_H,
    FILE_NB
};

enum Rank : int {
    RANK_1,
    RANK_2,
    RANK_3,
    RANK_4,
    RANK_5,
    RANK_6,
    RANK_7,
    RANK_8,
    RANK_NB
};

enum Square : int {
    A1, B1, C1, D1, E1, F1, G1, H1,
    A2, B2, C2, D2, E2, F2, G2, H2,
    A3, B3, C3, D3, E3, F3, G3, H3,
    A4, B4, C4, D4, E4, F4, G4, H4,
    A5, B5, C5, D5, E5, F5, G5, H5,
    A6, B6, C6, D6, E6, F6, G6, H6,
    A7, B7, C7, D7, E7, F7, G7, H7,
    A8, B8, C8, D8, E8, F8, G8, H8,
    SQ_NONE,
    SQ_NB = 64
};

enum MoveType {
    NORMAL,
    PROMOTION = 1 << 14,
    EN_PASSANT = 2 << 14,
    CASTLING = 3 << 14
};

// Store the score as a midgame and endgame variation,
// these scores are later interpolated based on the game state.
struct PhaseScore {
    Value mid = 0;
    Value end = 0;

    // Interpolate between the two using a gamephase
    Value interp(Value phase) {
        return (mid * phase + (end * (128 - phase))) / 128;
    };

    // Operator to add or subtract phase scores
    PhaseScore operator+(PhaseScore p) { return {mid + p.mid, end + p.end}; }
    PhaseScore operator-(PhaseScore p) { return {mid - p.mid, end + p.end}; }
    PhaseScore operator+=(PhaseScore p) { return {mid = mid + p.mid, end = end + p.end}; }
    PhaseScore operator-=(PhaseScore p) { return {mid = mid - p.mid, end = end - p.end}; }
    PhaseScore operator-() { return {-mid, -end}; }

    // Operator for multiplying by a coefficient
    PhaseScore operator*(Value v) { return {mid * v, end * v}; }
};

// A move must be stored in 16 bits
// Bits [0,5]: origin square
// Bits [6,11]: target square
// Bits [12,13]: promotion piece type (knight, bishop, rook, or queen)
// Bits [14-15]: flag for en-passant, promotion or castling
//
// A null move will be one with all the bits unset since a piece cannot
// go from square 0 to square 0 in a normal chess game.

class Move {
public:
    // Create the constructors for a move
    Move() = default;
    Move(int16_t m) { m_move = m; }
    Move(Square from, Square to) { m_move = from + (to << 6); }
    Move(Square from, Square to, MoveType type, PieceType pt = KNIGHT) {
        m_move = type + ((pt - KNIGHT) << 12) + from + (to << 6);
    }

    static Move null() { return Move(65); }
    static Move none() { return Move(0); }

    // Functions to access move data
    constexpr Square from() const { return Square(m_move & 0x3F); }
    constexpr Square to() const { return Square((m_move >> 6) & 0x3F); }
    constexpr int from_to() const { return m_move & 0xFFF; }
    constexpr MoveType type() const { return MoveType(m_move & (3 << 14)); }
    constexpr PieceType promotion() const { return PieceType(((m_move >> 12) & 3) + KNIGHT); }
    constexpr bool is_ok() const { return from() != to(); }
    constexpr bool is_null() const { return m_move == 65; }

    constexpr bool operator==(const Move& m) const { return m_move == m.m_move; }
    constexpr bool operator!=(const Move& m) const { return m_move != m.m_move; }

    constexpr uint16_t data() const { return m_move; }

protected:
    uint16_t m_move;
};

// Define aligned allocation
inline void* aligned_malloc(size_t alignment, size_t size) {
    // Ensure size and alignment are non-zero with alignment smaller than size
    assert(size && alignment && size > alignment);
    // Setup specific instructions
    #if defined(_WIN32) || defined(WIN32)
    return _aligned_malloc(size, alignment);
    #else
    return std::aligned_alloc(alignment, size);
    #endif
}

// Define aligned free
inline void aligned_free(void* ptr) {
    assert(ptr);
    // Setup specific instructions
    #if defined(_WIN32) || defined(WIN32)
    _aligned_free(ptr);
    #else
    free(ptr);
    #endif
}

#define ENABLE_INCREMENT_OPERATORS(T) \
    inline T& operator++(T& d) { return d = T(int(d) + 1); } \
    inline T& operator--(T& d) { return d = T(int(d) - 1); }

// Allow incrementing on piecetypes, squares, files and ranks
ENABLE_INCREMENT_OPERATORS(PieceType)
ENABLE_INCREMENT_OPERATORS(Square)
ENABLE_INCREMENT_OPERATORS(File)
ENABLE_INCREMENT_OPERATORS(Rank)
ENABLE_INCREMENT_OPERATORS(CastlingRights)

#undef ENABLE_INCREMENT_OPERATORS

// Operator to add or subtract phase scores from values, this just defaults to the midgame value
constexpr Value operator+(Value v, PhaseScore p) { return p.mid + v; }
constexpr Value operator-(Value v, PhaseScore p) { return p.mid - v; }
inline Value& operator+=(Value& v, PhaseScore p) { return v = v + p.mid; }
inline Value& operator-=(Value& v, PhaseScore p) { return v = v - p.mid; }

// Allow adding two directions together
constexpr Direction operator+(Direction d1, Direction d2) { return Direction(int(d1) + int(d2)); }
constexpr Direction operator-(Direction d1, Direction d2) { return Direction(int(d1) - int(d2)); }

// Allow adding a direction to a square
constexpr Square operator+(Square s, Direction d) { return Square(int(s) + int(d)); }
constexpr Square operator-(Square s, Direction d) { return Square(int(s) - int(d)); }
inline Square& operator+=(Square& s, Direction d) { return s = s + d; }
inline Square& operator-=(Square& s, Direction d) { return s = s - d; }

// Toggle color
constexpr Color operator~(Color c) { return Color(c ^ BLACK); }

// Swap piece color
constexpr Piece operator~(Piece pc) { return Piece(pc ^ 8); }

// Check castling rights for a given side
constexpr CastlingRights operator&(Color c, CastlingRights rights) {
    return CastlingRights((c == WHITE ? WHITE_CASTLE : BLACK_CASTLE) & rights);
}

// Check castle rights are ok
constexpr bool castle_rights_ok(CastlingRights rights) {
    return rights >= NO_CASTLE && rights <= ANY_CASTLE;
}

// Rank and File functions
constexpr Square flip_rank(Square s) { return Square(s ^ A8); }
constexpr Square flip_file(Square s) { return Square(s ^ H1); }
constexpr Rank rank_of(Square s) { return Rank(s >> 3); }
constexpr File file_of(Square s) { return File(s & 7); }
constexpr Rank relative_rank(Color c, Rank r) { return Rank(r ^ (c * 7)); }
constexpr Rank relative_rank(Color c, Square s) { return relative_rank(c, rank_of(s)); }

// Square functions
constexpr Square make_square(Rank r, File f) { return Square((r << 3) + f); }
constexpr Square relative_square(Color c, Square s) { return Square(s ^ (c * 56)); }
constexpr Direction pawn_push(Color c) { return c == WHITE ? NORTH : SOUTH; }
constexpr bool is_ok(Square s) { return s >= A1 && s <= H8; }

// Piece functions
constexpr PieceType piece_type(Piece pc) { return PieceType(pc & 7); }
constexpr Piece make_piece(Color c, PieceType pt) { return Piece((c << 3) + pt); }
constexpr Color piece_color(Piece pc) {
    assert(pc != NO_PIECE);
    return Color(pc >> 3);
}

// Piece value function
constexpr PhaseScore piece_value(PieceType pt) {
    switch (pt) {
        case PAWN: return {125, 210};
        case KNIGHT: return {780, 850};
        case BISHOP: return {825, 915};
        case ROOK: return {1275, 1380};
        case QUEEN: return {2540, 2680};
        default: return {VALUE_ZERO, VALUE_ZERO};
    }
}

constexpr PhaseScore piece_value(Piece pc) {
    return piece_value(piece_type(pc));
}

// Mating value functions
constexpr Value mate_in(int ply) { return VALUE_MATE - ply; }
constexpr Value mated_in(int ply) { return -VALUE_MATE + ply; }

// Values to and and from transposition table
constexpr Value value_from_tt(Value v, int ply, int fifty) {
    if (v == VALUE_NONE) return VALUE_NONE;
    // Handle winning values
    if (v >= VALUE_WIN_MAX_PLY) {
        // Check for a potentially wrong mate score
        if (v >= VALUE_MATE_IN_MAX_PLY && VALUE_MATE - v > 100 - fifty)
            return VALUE_WIN_MAX_PLY - 1;

        // Check for a potentially wrong winning score
        if (VALUE_WIN - v > 100 - fifty)
            return VALUE_WIN_MAX_PLY - 1;

        // Return normally
        return v - ply;
    }

    // Handle losing values
    if (v <= VALUE_LOSS_MAX_PLY) {
        // Check for a potentially wrong mate score
        if (v <= VALUE_MATED_IN_MAX_PLY && VALUE_MATE + v > 100 - fifty)
            return VALUE_LOSS_MAX_PLY + 1;

        // Check for a potentially wrong winning score
        if (VALUE_WIN + v > 100 - fifty)
            return VALUE_LOSS_MAX_PLY + 1;

        // Return normally
        return v + ply;
    }

    // Return score if not a winning/losing score
    return v;
}

constexpr Value value_to_tt(Value v, int ply) {
    assert(v != VALUE_NONE);
    return v >= VALUE_WIN_MAX_PLY ? v + ply
            : v <= VALUE_LOSS_MAX_PLY ? v - ply : v;

}

constexpr bool is_win(Value v) {
    return v > VALUE_WIN_MAX_PLY;
}

constexpr bool is_loss(Value v) {
    return v < VALUE_LOSS_MAX_PLY;
}

constexpr bool is_extremity(Value v) {
    return is_win(v) || is_loss(v);
}

constexpr Value clamp_score(Value v) {
    return std::clamp(v, static_cast<Value>(VALUE_WIN_MAX_PLY), static_cast<Value>(VALUE_LOSS_MAX_PLY));
}

}

#endif
