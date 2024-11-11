#include "evaluate.hpp"
#include "position.hpp"
#include "types.hpp"

namespace Stella {

namespace Evaluate {

Value phase;
Color us;
Color them;

PhaseScore val(Color c, PhaseScore v) {
    return (c == us) ? v : -v;
}

void init() {

}

namespace Tables {

// // Piece square tables for positional scores
PhaseScore psqt[PIECE_TYPE_NB][SQ_NB] = {
    {},
{
    {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0},
    {-40,10}, {0,10}, {-20,10}, {-20,10}, {-20,10}, {20,0}, {40,0}, {-20,-10},
    {-30,0}, {0,10}, {0,-10}, {-10,0}, {0,0}, {0,0}, {30,0}, {-10,-10},
    {-30,10}, {0,10}, {0,0}, {10,-10}, {20,-10}, {10,-10}, {10,0}, {-20,0},
    {-10,30}, {10,20}, {10,10}, {20,0}, {20,0}, {10,0}, {20,20}, {-20,20},
    {-10,90}, {10,100}, {30,80}, {30,70}, {60,60}, {60,50}, {20,80}, {-20,80},
    {100,180}, {130,170}, {60,160}, {100,130}, {70,150}, {130,130}, {30,160}, {-10,190},
    {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0},
},
{
    {-100,-30}, {-20,-50}, {-60,-20}, {-30,-20}, {-20,-20}, {-30,-20}, {-20,-50}, {-20,-60},
    {-30,-40}, {-50,-20}, {-10,-10}, {0,0}, {0,0}, {20,-20}, {-10,-20}, {-20,-40},
    {-20,-20}, {-10,0}, {10,0}, {10,20}, {20,10}, {20,0}, {20,-20}, {-20,-20},
    {-10,-20}, {0,-10}, {20,20}, {10,20}, {30,20}, {20,20}, {20,0}, {-10,-20},
    {-10,-20}, {20,0}, {20,20}, {50,20}, {40,20}, {70,10}, {20,10}, {20,-20},
    {-50,-20}, {60,-20}, {40,10}, {60,10}, {80,0}, {130,-10}, {70,-20}, {40,-40},
    {-70,-20}, {-40,-10}, {70,-20}, {40,0}, {20,-10}, {60,-20}, {10,-20}, {-20,-50},
    {-170,-60}, {-90,-40}, {-30,-10}, {-50,-30}, {60,-30}, {-100,-30}, {-20,-60}, {-110,-100},
},
{
    {-30,-20}, {0,-10}, {-10,-20}, {-20,0}, {-10,-10}, {-10,-20}, {-40,0}, {-20,-20},
    {0,-10}, {20,-20}, {20,-10}, {0,0}, {10,0}, {20,-10}, {30,-20}, {0,-30},
    {0,-10}, {20,0}, {20,10}, {20,10}, {10,10}, {30,0}, {20,-10}, {10,-20},
    {-10,-10}, {10,0}, {10,10}, {30,20}, {30,10}, {10,10}, {10,0}, {0,-10},
    {0,0}, {0,10}, {20,10}, {50,10}, {40,10}, {40,10}, {10,0}, {0,0},
    {-20,0}, {40,-10}, {40,0}, {40,0}, {40,0}, {50,10}, {40,0}, {0,0},
    {-30,-10}, {20,0}, {-20,10}, {-10,-10}, {30,0}, {60,-10}, {20,0}, {-50,-10},
    {-30,-10}, {0,-20}, {-80,-10}, {-40,-10}, {-20,-10}, {-40,-10}, {10,-20}, {-10,-20},
},
{
    {-20,-10}, {-10,0}, {0,0}, {20,0}, {20,0}, {10,-10}, {-40,0}, {-30,-20},
    {-40,-10}, {-20,-10}, {-20,0}, {-10,0}, {0,-10}, {10,-10}, {-10,-10}, {-70,0},
    {-40,0}, {-20,0}, {-20,0}, {-20,0}, {0,-10}, {0,-10}, {0,-10}, {-30,-20},
    {-40,0}, {-30,0}, {-10,10}, {0,0}, {10,0}, {-10,-10}, {10,-10}, {-20,-10},
    {-20,0}, {-10,0}, {10,10}, {30,0}, {20,0}, {40,0}, {-10,0}, {-20,0},
    {0,10}, {20,10}, {30,10}, {40,0}, {20,0}, {40,0}, {60,0}, {20,0},
    {30,10}, {30,10}, {60,10}, {60,10}, {80,0}, {70,0}, {30,10}, {40,0},
    {30,10}, {40,10}, {30,20}, {50,20}, {60,10}, {10,10}, {30,10}, {40,0},
},
{
    {0,-30}, {-20,-30}, {-10,-20}, {10,-40}, {-20,0}, {-20,-30}, {-30,-20}, {-50,-40},
    {-40,-20}, {-10,-20}, {10,-30}, {0,-20}, {10,-20}, {20,-20}, {0,-40}, {0,-30},
    {-10,-20}, {0,-30}, {-10,20}, {0,10}, {0,10}, {0,20}, {10,10}, {0,0},
    {-10,-20}, {-30,30}, {-10,20}, {-10,50}, {0,30}, {0,30}, {0,40}, {0,20},
    {-30,0}, {-30,20}, {-20,20}, {-20,40}, {0,60}, {20,40}, {0,60}, {0,40},
    {-10,-20}, {-20,10}, {10,10}, {10,50}, {30,50}, {60,40}, {50,20}, {60,10},
    {-20,-20}, {-40,20}, {0,30}, {0,40}, {-20,60}, {60,20}, {30,30}, {50,0},
    {-30,-10}, {0,20}, {30,20}, {10,30}, {60,30}, {40,20}, {40,10}, {40,20},
},
{
    {-20,-50}, {40,-30}, {10,-20}, {-50,-10}, {10,-30}, {-30,-10}, {20,-20}, {10,-40},
    {0,-30}, {10,-10}, {-10,0}, {-60,10}, {-40,10}, {-20,0}, {10,0}, {10,-20},
    {-10,-20}, {-10,0}, {-20,10}, {-50,20}, {-40,20}, {-30,20}, {-20,10}, {-30,-10},
    {-50,-20}, {0,0}, {-30,20}, {-40,20}, {-50,30}, {-40,20}, {-30,10}, {-50,-10},
    {-20,-10}, {-20,20}, {-10,20}, {-30,30}, {-30,30}, {-20,30}, {-10,30}, {-40,0},
    {-10,10}, {20,20}, {0,20}, {-20,20}, {-20,20}, {10,40}, {20,40}, {-20,10},
    {30,-10}, {0,20}, {-20,10}, {-10,20}, {-10,20}, {0,40}, {-40,20}, {-30,10},
    {-60,-70}, {20,-40}, {20,-20}, {-20,-20}, {-60,-10}, {-30,20}, {0,0}, {10,-20},
}
};

};

template<>
PhaseScore evaluate_piece<PAWN>(Position* pos) {
    // Get the occupied board for this piece
    Bitboard occupied = pos->pieces(PAWN);

    // Loop through all pawns on the board
    Square sq;
    PhaseScore v;

    while (occupied) {
        sq = pop_lsb(occupied);
        // Get the piece on the square
        Piece pc = pos->piece_on(sq);
        Color c = piece_color(pc);

        // Set the square to be relative
        sq = relative_square(c, sq);

        // Add up the pieces material score
        PhaseScore mat = piece_value(pc);
        v += val(c, mat);

        // Add the pieces psqt score
        PhaseScore psqt = Tables::psqt[PAWN][sq];
        v += val(c, psqt);

    }

    // Return final value
    return v;
}

template<>
PhaseScore evaluate_piece<KNIGHT>(Position* pos) {
    // Get the occupied board for this piece
    Bitboard occupied = pos->pieces(KNIGHT);

    // Loop through all knights on the board
    Square sq;
    PhaseScore v;

    while (occupied) {
        sq = pop_lsb(occupied);
        // Get the piece on the square
        Piece pc = pos->piece_on(sq);
        Color c = piece_color(pc);

        // Set the square to be relative
        sq = relative_square(c, sq);

        // Add up the pieces material score
        PhaseScore mat = piece_value(pc);
        v += val(c, mat);

        // Add the pieces psqt score
        PhaseScore psqt = Tables::psqt[KNIGHT][sq];
        v += val(c, psqt);

    }

    // Return final value
    return v;
}

template<>
PhaseScore evaluate_piece<BISHOP>(Position* pos) {
    // Get the occupied board for this piece
    Bitboard occupied = pos->pieces(BISHOP);

    // Loop through all bishops on the board
    Square sq;
    PhaseScore v;

    while (occupied) {
        sq = pop_lsb(occupied);
        // Get the piece on the square
        Piece pc = pos->piece_on(sq);
        Color c = piece_color(pc);

        // Set the square to be relative
        sq = relative_square(c, sq);

        // Add up the pieces material score
        PhaseScore mat = piece_value(pc);
        v += val(c, mat);

        // Add the pieces psqt score
        PhaseScore psqt = Tables::psqt[BISHOP][sq];
        v += val(c, psqt);

    }

    // Return final value
    return v;
}

template<>
PhaseScore evaluate_piece<ROOK>(Position* pos) {
    // Get the occupied board for this piece
    Bitboard occupied = pos->pieces(ROOK);

    // Loop through all rooks on the board
    Square sq;
    PhaseScore v;

    while (occupied) {
        sq = pop_lsb(occupied);
        // Get the piece on the square
        Piece pc = pos->piece_on(sq);
        Color c = piece_color(pc);

        // Set the square to be relative
        sq = relative_square(c, sq);

        // Add up the pieces material score
        PhaseScore mat = piece_value(pc);
        v += val(c, mat);

        // Add the pieces psqt score
        PhaseScore psqt = Tables::psqt[ROOK][sq];
        v += val(c, psqt);
    }

    // Return final value
    return v;
}

template<>
PhaseScore evaluate_piece<QUEEN>(Position* pos) {
    // Get the occupied board for this piece
    Bitboard occupied = pos->pieces(QUEEN);

    // Loop through all queens on the board
    Square sq;
    PhaseScore v;

    while (occupied) {
        sq = pop_lsb(occupied);
        // Get the piece on the square
        Piece pc = pos->piece_on(sq);
        Color c = piece_color(pc);

        // Set the square to be relative
        sq = relative_square(c, sq);

        // Add up the pieces material score
        PhaseScore mat = piece_value(pc);
        v += val(c, mat);

        // Add the pieces psqt score
        PhaseScore psqt = Tables::psqt[QUEEN][sq];
        v += val(c, psqt);
    }

    // Return final value
    return v;
}

template<>
PhaseScore evaluate_piece<KING>(Position* pos) {
    // Get the occupied board for this piece
    Bitboard occupied = pos->pieces(KING);

    // Loop through all queens on the board
    Square sq;
    PhaseScore v;

    while (occupied) {
        sq = pop_lsb(occupied);
        // Get the piece on the square
        Piece pc = pos->piece_on(sq);
        Color c = piece_color(pc);

        // Set the square to be relative
        sq = relative_square(c, sq);

        // Add the pieces psqt score
        PhaseScore psqt = Tables::psqt[KING][sq];
        v += val(c, psqt);
    }

    // Return final value
    return v;
}

Value evaluate(Position* pos) {
    // Set the sides to move
    us = pos->side();
    them = ~us;
    // Get the phase score
    phase = pos->game_phase();

    // Call evaluate for each piece type
    PhaseScore score;

    score += evaluate_piece<PAWN>(pos);
    score += evaluate_piece<KNIGHT>(pos);
    score += evaluate_piece<BISHOP>(pos);
    score += evaluate_piece<ROOK>(pos);
    score += evaluate_piece<QUEEN>(pos);
    score += evaluate_piece<KING>(pos);

    // Return the interpolated score
    return score.interp(phase);
}

}

}
