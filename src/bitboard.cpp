#include "bitboard.hpp"
#include "misc.hpp"

#include <cstring>
#include <limits>

namespace Stella {

uint8_t SquareDistance[SQ_NB][SQ_NB];
Bitboard BetweenBB[SQ_NB][SQ_NB];
Bitboard LineBB[SQ_NB][SQ_NB];
Bitboard PseudoAttacks[PIECE_TYPE_NB][SQ_NB];
Bitboard PawnAttacks[COLOR_NB][SQ_NB];

Bitboard RookAttacks[102400];
Bitboard BishopAttacks[5248];

Magic RookMagics[SQ_NB];
Magic BishopMagics[SQ_NB];

void init_magics(PieceType pt, Bitboard attacks[], Magic magics[]);

Bitboard safe_direction(Square s, int direction);

Bitboard sliding_attack(PieceType pt, Square s, Bitboard occupied);
Bitboard sliding_attack(PieceType pt, Square s);

// Return a bitboard of the target square for a direction from the square.
// If the step is illegal return an empty board.
Bitboard safe_direction(Square s, int direction) {
  assert(is_ok(s));
  Square to = Square(s + direction);
  return is_ok(to) && distance(s, to) <= 2 ? square_bb(to) : Bitboard(0);
}

std::string Bitboards::print(Bitboard b) {
  // Create a string to add to
  std::string s = "+-----------------+\n";
  // Loop through every rank and file
  for (Rank r = RANK_8; r >= RANK_1; --r) {
    s += "|";
    for (File f = FILE_A; f <= FILE_H; ++f) {
      s += b & make_square(r, f) ? " x" : " .";
    }
    s += " | ";
    s += std::to_string(1 + r);
    s += "\n";
  }
  s += "+-----------------+\n";
  s += "  a b c d e f g h\n";
  return s;
}

void Bitboards::init() {
  // Store directions the king and knight can move to quickly generate the moves
  int KingDirections[8]   = {-9, -8, -7, -1, 1, 7, 8, 9};
  int KnightDirections[8] = {-17, -15, -10, -6, 6, 10, 15, 17};
  PieceType SliderTypes[2] = {BISHOP, ROOK};
  // Setup square distances
  for (Square s1 = A1; s1 <= H8; ++s1)
    for (Square s2 = A1; s2 <= H8; ++s2)
      SquareDistance[s1][s2] = uint8_t(std::max(rank_distance(s1, s2), file_distance(s1, s2)));

  // Initialize magic bitboards
  init_magics(ROOK, RookAttacks, RookMagics);
  init_magics(BISHOP, BishopAttacks, BishopMagics);

  // Setup pseudo attacks and line/between bitboards
  for (Square s1 = A1; s1 <= H8; ++s1) {
    PawnAttacks[WHITE][s1] = pawn_attacks_bb(WHITE, square_bb(s1));
    PawnAttacks[BLACK][s1] = pawn_attacks_bb(BLACK, square_bb(s1));

    for (int direction : KingDirections)
      PseudoAttacks[KING][s1] |= safe_direction(s1, direction);
    for (int direction : KnightDirections)
      PseudoAttacks[KNIGHT][s1] |= safe_direction(s1, direction);

    PseudoAttacks[QUEEN][s1] = PseudoAttacks[BISHOP][s1] = attacks_bb(s1, BISHOP, 0ULL);
    PseudoAttacks[QUEEN][s1] |= PseudoAttacks[ROOK][s1] = attacks_bb(s1, ROOK, 0ULL);

    for (PieceType pt : SliderTypes) {
      for (Square s2 = A1; s2 <= H8; ++s2) {
        if (PseudoAttacks[pt][s1] & s2) {
          LineBB[s1][s2] = (attacks_bb(s1, pt, 0) & attacks_bb(s2, pt, 0)) | s1 | s2;
          BetweenBB[s1][s2] = (attacks_bb(s1, pt, square_bb(s2)) & attacks_bb(s2, pt, square_bb(s1)));
        }
        BetweenBB[s1][s2] |= s2;
      }
    }
  }
}

Bitboard sliding_attack(PieceType pt, Square s, Bitboard occupied) {
  assert(pt == ROOK || pt == BISHOP);
  // Store the directions the two sliders move in
  Bitboard attacks = 0;
  Direction RookDirections[4] = {NORTH, SOUTH, EAST, WEST};
  Direction BishopDirections[4] = {NORTH_EAST, NORTH_WEST, SOUTH_EAST, SOUTH_WEST};
  // Loop over the directions for the selected slider
  for (Direction d : (pt == ROOK ? RookDirections : BishopDirections)) {
    // Store the square in a variable that can be changed
    Square sq = s;
    // Loop over every direction until the edge of the board is reached
    while (safe_direction(sq, d)) {
      attacks |= (sq += d);
      // If an occupied square is reached don't proceed
      if (occupied & sq) break;
    }
  }
  // Return the attacks
  return attacks;
}

Bitboard sliding_attack(PieceType pt, Square s) {
  return sliding_attack(pt, s, 0);
}

void init_magics(PieceType pt, Bitboard attacks[], Magic magics[]) {
  // Use these optimal seeds to generate the magic numbers.
  // Retrieved from brute forcing random seeds and choosing one with lowest steps per square.
  uint16_t seeds[SQ_NB] =
  { 18512, 53,    49288, 34962, 53536, 33290, 9256,  34980,
    8498,  1159,  18694, 652,   4234,  11794, 24614, 33948,
    25105, 33042, 17413, 6288,  32978, 5147,  5153,  12545,
    6172,  8392,  9288,  8624,  21889, 50432, 2328,  8354,
    35592, 8616,  61704, 5268,  8290,  1122,  8275,  1305,
    1539,  4992,  17456, 28928, 1569,  1418,  35968, 20500,
    169,   11520, 1172,  24620, 12576, 36997, 33027, 8360,
    1161,  49298, 34864, 4865,  19464, 8239,  33293, 4290 };

  // Create empty tables used during generation.
  // 4096 entries since that is the maximum which occurs when a rook is in a corner.
  Bitboard occupancy[4096];
  Bitboard reference[4096];
  Bitboard edges;
  Bitboard b;
  // Initialize other values
  int epoch[4096] = {};
  int cnt = 0;
  int size = 0;

  // Loop through every square
  for (Square s = A1; s <= H8; ++s) {
    // Find the board edges to remove them from occupancy,
    // since those do not need to be considered for magic bitboards
    edges = ((RANK_1BB | RANK_8BB) & ~rank_bb(s)) | ((FILE_ABB | FILE_HBB) & ~file_bb(s));

    // The size of the entry for the given square must contain every possible
    // attack for every occupancy for that piece, since the square can either
    // be 1 or 0, it's easy to deduce the total size is 2 to the power of
    // the number of squares attacked by the piece (not including edges).
    // Therefore the shift value must be 64 - size.
    Magic& m = magics[s];
    m.mask = sliding_attack(pt, s) & ~edges;
    m.shift = 64 - popcount(m.mask);

    // Set the size for the attack boards, start at the beginning
    // and increment from that index using the size determined from generation
    m.attacks = s == A1 ? attacks : magics[s - 1].attacks + size;

    // Reset the size and board to zero before generation
    b = 0;
    size = 0;

    // Using the Carry Rippler trick to traverse all subsets of a set
    // which can be found at https://www.chessprogramming.org/Traversing_Subsets_of_a_Set
    do {
      occupancy[size] = b;
      reference[size] = sliding_attack(pt, s, b);

      if (hasPext) m.attacks[pext(b, m.mask)] = reference[size];

      size++;
      b = (b - m.mask) & m.mask;
    } while (b);

    // If Pext is enabled no need to do more
    if (hasPext) continue;

    // Create a new Random object that is used to generate magics
    Random rng(seeds[s]);

    // Find a magic for the given square by choosing a pseudo-random
    // number until it satisfies the magic number conditions.
    for (int i = 0; i < size;) {
      // Continue generating magics until less than 6 upper bits,
      // can check this by bitshifting all other bits out and counting the result.
      for (m.magic = 0; popcount((m.magic * m.mask) >> 56) < 6;)
        m.magic = rng.random_sparse<Bitboard>();

      // Verify the magic works with every possible occupancy of the given index
      // so that every sliding attack is generated correctly.
      for (++cnt, i = 0; i < size; ++i) {
        // Get the index for the current occupancy board
        uint32_t idx = m.index(occupancy[i]);

        // Check if the magic number satisfies the conditions,
        // if the number of tries on this index is less than the current count
        // then overwrite the number of tries and change the attacks.
        if (epoch[idx] < cnt) {
          epoch[idx] = cnt;
          m.attacks[idx] = reference[i];
        }
        // If the attacks board has not been previously written as the reference then break.
        else if (m.attacks[idx] != reference[i])
          break;
      }
    }
  }
}

}
