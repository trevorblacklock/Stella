#include <iostream>

#include "movegen.hpp"
#include "position.hpp"
#include "misc.hpp"

namespace Stella {

Move Generator::next() {
  // Switch statement for every stage
    switch (generationStage) {
        // For initial TT_MOVE stage
        case TT_MOVE:
            // Increment the stage
            ++generationStage;
            // Return the tt move so long as it is pseudo legal
            if (pos->is_pseudolegal(ttMove)) return ttMove;
            [[fallthrough]];

        // Initialize the captures
        case INIT_CAPTURES:
            // Increment the stage
            ++generationStage;
            // Generate all capture moves
            generate<CAPTURES>();
            [[fallthrough]];

        // Run through good captures
        case GOOD_CAPTURES:
            // Loop through all the good captures
            if (captureIdx < goodCaptures) return next_best<CAPTURES>();
            // If in qsearch mode exit
            if (mode == QSEARCH) {
                if (captureIdx < captures.size) return next_best<CAPTURES>();
                break;
            }
            // If in qsearch with checks move to evasions
            if (mode == QSEARCH_CHECK) {
                generationStage = INIT_EVASIONS;
                next();
            }
            // Increment the stage
            ++generationStage;
            [[fallthrough]];

        // Initialize the quiets
        case INIT_QUIETS:
            // Increment the stage
            ++generationStage;
            // Generate all quiet moves
            generate<QUIETS>();
            [[fallthrough]];

        // Get the good quiets
        case GOOD_QUIETS:
            // Placeholder until move ordering
            if (quietIdx < quiets.size) return next_best<QUIETS>();
            // Increment the stage
            ++generationStage;
            [[fallthrough]];

        // Get the bad captures
        case BAD_CAPTURES:
            // Loop through the rest of the captures
            if (captureIdx < captures.size) return next_best<CAPTURES>();
            // Increment the stage
            ++generationStage;
            [[fallthrough]];

        // Get the bad quiets
        case BAD_QUIETS:
            // Increment the stage
            ++generationStage;
            break;

        // Initialize the evasions
        case INIT_EVASIONS:
            // Increment the stage
            ++generationStage;
            // Generate all evasion moves
            generate<EVASIONS>();
            [[fallthrough]];

        // Get all the evasions
        case ALL_EVASIONS:
            // Return the next evasion move which is stored as a quiet
            if (quietIdx < quiets.size) return next_best<QUIETS>();
            // Break out of loop once all evasions are searched
            break;
    }

    // By default return Move::none() to know when no more moves are left
    return Move::none();
}

Generator::Generator(Position* p) {
    // Initialize the generator object
    pos = p;
    side = pos->side();
    // Set the generation stage
    generationStage = TT_MOVE;
    // Set the gen mode to perft
    mode = PERFT;
    // Generate the mask
    generate_mask();
    // Generate all legal moves
    generate<LEGAL>();
}

Generator::Generator(Position* p, GenerationMode m, Move tt) {
    // Initialize the generator object
    pos = p;
    side = pos->side();
    // Set the generation stage
    generationStage = TT_MOVE;
    // Store the tt move
    ttMove = tt;
    // Set the generation mode
    mode = m;
    // Generate the mask
    generate_mask();
}

inline void Generator::generate_mask() {
  // Assert the mask is set to all squares
  assert(mask == ALL_SQUARES);
  // Get the number of checks on the king
  uint8_t checkCount = popcount(pos->checks());
  // If check count is two then only king moves are possible so zero out the mask
  if (checkCount >= 2) mask = 0;
  else if (checkCount == 1) mask = between_bb(pos->ksq(side), lsb(pos->checks()));
}

// Selects the next best move for a given generation type
template<GenerationType T>
inline Move Generator::next_best() {
    // If the type is a capture
    if (T == CAPTURES) {
        // Set current best to current move in the list
        int best = captureIdx;
        // Look through moves in the list beyond the current,
        // and find the highest scoring one
        for (int idx = captureIdx + 1; idx < captures.size; ++idx)
            if (captures.scores[idx] > captures.scores[best]) best = idx;

        // Replace best score with the current index, the old index will remain
        // but is never looked at again
        Move m = captures.moves[best];
        see = seeScore[best];
        seeScore[best] = seeScore[captureIdx];
        captures.scores[best] = captures.scores[captureIdx];
        captures.moves[best] = captures.moves[captureIdx++];
        return m;
    }
    // If the type is a quiet
    if (T == QUIETS) {
        return quiets.moves[quietIdx++];
    }
    // For legal generation type return the next move immediately
    if (T == LEGAL) {
        if (captureIdx < captures.size) return captures.moves[captureIdx++];
        if (quietIdx < quiets.size) return quiets.moves[quietIdx++];
    }

    // By default return a no move
    return Move::none();
}

// Instantiate legal templates
template Move Generator::next_best<LEGAL>();

// Add a move to the generation type movelist
template<GenerationType T>
inline void Generator::add_move(Move m) {
    // Check the move is ok
    assert(m.is_ok());

    // For legal generation type don't bother with scoring
    if (mode == PERFT) {
        // Check the move is legal before adding it
        if (pos->is_legal(m)) {
            if (T == CAPTURES) captures.moves[captures.size++] = m;
            if (T == QUIETS || T == EVASIONS) quiets.moves[quiets.size++] = m;
        }
    }
    // First check that the move is not the tt move
    else if (m != ttMove) {
        if (T == CAPTURES) {
            // Get the static exchange evaluation of this move
            Value score = pos->see(m);
            seeScore[captures.size] = score;
            // If see is above 0, then consider it a good capture
            if (score >= 0) goodCaptures++;
            // Add this move to the captures list
            captures.moves[captures.size++] = m;
        }
        else if (T == QUIETS || T == EVASIONS) {
            // Add this move to the quiets list
            quiets.moves[quiets.size++] = m;
        }
    }
}

// Add a move to the searched type movelist
void Generator::add_searched(Move m) {
    searched.moves[searchedIdx++] = m;
    searched.size++;
}

// Generate all the pawn moves in the position
template<GenerationType T>
inline void Generator::generate_pawns() {
    // Ensure generation type is not evasions
    assert(T != EVASIONS);

    // Get the side to move
    Color us = pos->side();
    Color them = ~us;

    // Move squares
    Square from, to;

    // Create a bitboard for all friendly pawns
    Bitboard friendlyPawns = pos->pieces(us, PAWN);

    // Split the friendly pawns into ones that can promote and those that cannot
    Bitboard pawnsOnSeventh = friendlyPawns & (us == WHITE ? RANK_7BB : RANK_2BB);
    Bitboard pawnsNotOnSeventh = friendlyPawns & ~pawnsOnSeventh;

    // Create a bitboard to represent all opponents pieces
    Bitboard enemyPieces = pos->pieces(them);

    // Create a bitboard of all empty squares
    Bitboard emptySquares = ~pos->pieces();

    // Get the directions for friendly pawns
    Direction north = pawn_push(us);
    Direction north_east = (us == WHITE) ? NORTH_EAST : SOUTH_WEST;
    Direction north_west = (us == WHITE) ? NORTH_WEST : SOUTH_EAST;

    // Create placeholder boards
    Bitboard b, b1;

    // Create a bitboard for non empty push squares
    Bitboard b2 = shift(pawnsNotOnSeventh, north) & emptySquares;

    // Create a mask
    Bitboard pawnMask = mask;
    Bitboard promotionMask = mask & emptySquares;

    // If quiet generation then only look at moves that land on an empty square,
    // whereas if quiet generation then only look at moves that can capture
    if (T == QUIETS) pawnMask &= emptySquares;
    else if (T == CAPTURES) pawnMask &= enemyPieces;

    // Generate quiet moves
    if (T == QUIETS) {
        // Generate single pawn pushes
        b = b2 & pawnMask;
        // Generate double pawn pushes
        b1 = shift((b2 & (us == WHITE ? RANK_3BB : RANK_6BB)), north) & pawnMask;

        // Loop through and add single pawn pushes
        while (b) {
            to = pop_lsb(b);
            add_move<QUIETS>(Move(to - north, to));
        }
        // Loop through and add double pawn pushes
        while (b1) {
            to = pop_lsb(b1);
            add_move<QUIETS>(Move(to - north - north, to));
        }
    }

  // Generate captures
    if (T == CAPTURES) {
        // Generate eastward captures
        b = shift(pawnsNotOnSeventh, north_east) & pawnMask;
        // Loop through and add eastward captures
        while (b) {
            to = pop_lsb(b);
            add_move<CAPTURES>(Move(to - north_east, to));
        }
        // Generate westward captures
        b = shift(pawnsNotOnSeventh, north_west) & pawnMask;
        // Loop through and add westward captures
        while (b) {
            to = pop_lsb(b);
            add_move<CAPTURES>(Move(to - north_west, to));
        }

        // Generate promotions
        b = shift(pawnsOnSeventh, north) & promotionMask;
        // Loop through and add promotions
        while (b) {
            to = pop_lsb(b);
            for (PieceType pt = KNIGHT; pt <= QUEEN; ++pt) add_move<CAPTURES>(Move(to - north, to, PROMOTION, pt));
        }
        // Generate eastward capture promotions
        b = shift(pawnsOnSeventh, north_east) & pawnMask;
        // Loop through and add eastward capture promotions
        while (b) {
            to = pop_lsb(b);
            for (PieceType pt = KNIGHT; pt <= QUEEN; ++pt) add_move<CAPTURES>(Move(to - north_east, to, PROMOTION, pt));
        }
        // Generate westward capture promotions
        b = shift(pawnsOnSeventh, north_west) & pawnMask;
        // Loop through and add westward capture promotions
        while (b) {
            to = pop_lsb(b);
            for (PieceType pt = KNIGHT; pt <= QUEEN; ++pt) add_move<CAPTURES>(Move(to - north_west, to, PROMOTION, pt));
        }

        // Generate enpassant
        if (pos->ep_square() != SQ_NONE) {
            // Create a board of candidates
            b = pawnsNotOnSeventh & pawn_attacks(them, pos->ep_square());
            // Loop through board as there can only be two pawns with ability to enpassant
            while (b) {
                from = pop_lsb(b);
                add_move<CAPTURES>(Move(from, pos->ep_square(), EN_PASSANT));
            }
        }
  }

}

// Generate all the piece moves in the position
template<GenerationType T>
inline void Generator::generate_piece(PieceType pt) {
    // Do not generate pawn moves here
    assert(pt != PAWN);

    // Get side to move
    Color us = side;
    Color them = ~us;

    // Move squares
    Square from, to;

    // Create a board of friendly and enemy pieces
    Bitboard friendlyPieces = pos->pieces(us, pt);
    Bitboard enemyPieces = pos->pieces(them);

    // Create a board of empty squares
    Bitboard emptySquares = ~pos->pieces();

    // If generating king moves
    if (pt == KING) {
        // Create a custom mask for king moves, by default will be all empty squares
        Bitboard kingMask = emptySquares;
        // For captures must land on an enemy piece
        if (T == CAPTURES) kingMask = enemyPieces;

        // From square will always be king square
        from = pos->ksq(us);

        // Generate king attacks
        Bitboard attacks = attacks_bb(from, KING) & kingMask;

        // Loop through king attacks and add the moves
        while (attacks) {
        to = pop_lsb(attacks);
        add_move<T>(Move(from, to));
        }

        // Generate all castling moves, making sure there are no checks
        if (T == QUIETS && !pos->checks()) {
        // Find the rights for the side to move
        CastlingRights kingRights = us == WHITE ? WHITE_KING : BLACK_KING;
        CastlingRights queenRights = us == WHITE ? WHITE_QUEEN : BLACK_QUEEN;

        // Now determine if castling is possible on kingside
        if (pos->can_castle(kingRights) && !pos->castling_blocked(kingRights))
            add_move<QUIETS>(Move(from, pos->castle_rook_square(kingRights), CASTLING));

        // Now determine if castling is possible on queenside
        if (pos->can_castle(queenRights) && !pos->castling_blocked(queenRights))
            add_move<QUIETS>(Move(from, pos->castle_rook_square(queenRights), CASTLING));
        }

    // Return once finished with king moves to not generate other moves on accident
    return;
  }

    // Create a mask for the pieces
    Bitboard pieceMask = mask;

    // If quiet generation then only look at moves that land on an empty square,
    // whereas if quiet generation then only look at moves that can capture
    if (T == QUIETS) pieceMask &= emptySquares;
    else if (T == CAPTURES) pieceMask &= enemyPieces;

    // Loop through every friendly piece
    while (friendlyPieces) {
        from = pop_lsb(friendlyPieces);
        // Generate an attack board for the given piece
        Bitboard attacks = attacks_bb(from, pt, pos->pieces()) & pieceMask;

        // Loop through every square and add the move
        while (attacks) {
            to = pop_lsb(attacks);
            add_move<T>(Move(from, to));
        }
    }
}

// Generate all moves of the given type
template<GenerationType T>
inline void Generator::generate() {
    // Check for different generation types
    if (T == LEGAL) {
        generate<CAPTURES>();
        generate<QUIETS>();
    }
    else if (T == EVASIONS) {
        generate_piece<T>(KING);
    }
    else if (T == CAPTURES || T == QUIETS) {
        generate_pawns<T>();
        generate_piece<T>(KNIGHT);
        generate_piece<T>(BISHOP);
        generate_piece<T>(ROOK);
        generate_piece<T>(QUEEN);
        generate_piece<T>(KING);
    }
}

}
