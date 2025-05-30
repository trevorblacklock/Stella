#ifndef MOVEGEN_H_INCLUDED
#define MOVEGEN_H_INCLUDED

#include "types.hpp"
#include "bitboard.hpp"
#include "history.hpp"

namespace Stella {

class Position;

// Type of moves to generate
enum GenerationType {
    CAPTURES,
    QUIETS,
    EVASIONS,
    LEGAL
};

// Stages of move generation
enum GenerationStage {
    TT_MOVE,
    INIT_CAPTURES,
    GOOD_CAPTURES,
    KILLER1,
    KILLER2,
    INIT_QUIETS,
    GOOD_QUIETS,
    BAD_CAPTURES,
    BAD_QUIETS,
    INIT_EVASIONS,
    ALL_EVASIONS
};

enum GenerationMode {
    PV_SEARCH,
    QSEARCH,
    QSEARCH_CHECK,
    PERFT
};

// Store the moves and scores in a movelist
struct MoveList {
    Move     moves[MAX_MOVES];
    int      scores[MAX_MOVES];
    uint16_t size = 0;
};

class Generator {
private:
    // Movelists for quiet moves, capture moves and searched moves
    MoveList captures;
    MoveList quiets;
    MoveList searched;
    // Store the killer moves for the position
    Move killer1;
    Move killer2;
    // Store the see scores for a given move
    Value seeScore[MAX_MOVES];
    // Store the see score for the current move in the generator,
    // in search will not need to recalculate the see score for a small time save.
    Value see;
    // Number of captures and quiets considered good
    uint16_t goodCaptures = 0;
    uint16_t goodQuiets = 0;
    // Indices for all three move lists
    uint16_t captureIdx = 0;
    uint16_t quietIdx = 0;
    uint16_t searchedIdx = 0;
    // Store the generation stage and if scoring should be skipped
    uint8_t generationStage = TT_MOVE;
    // Store the generation mode
    GenerationMode mode;
    // Store the side to move
    Color side;
    // Store the ply of the search (if one is run)
    uint8_t ply;
    // Store a pointer to the active position
    Position* pos;
    // Store a pointer to the histories
    History* hist;
    // Store the passed move from the transposition table
    Move ttMove;
    // Whether quiet generation should be skipped, relevant in certain
    // pruning techniques used in the main search.
    bool skipQuiets = false;
    // A mask used for non-king move generation when in check. If in check the mask
    // will cover the squares between the attacker and king to only attempt generation
    // of moves that have the ability to block the check. This is done to speed up
    // generation and not waste time on moves that have no possibility of blocking the check.
    // This mask will be initialized whenever init() is called by using the passed position.
    Bitboard mask = ALL_SQUARES;

public:
    // Constructor for generator in perft
    Generator(Position* p);
    // Constsructor for generator used in search
    Generator(Position* p, History* h, GenerationMode m, Move tt, int ply);
    // Return the next move in generation
    Move next();
    // Find the next best move given the generation type
    template<GenerationType T>
    Move next_best();
    // Add a move to the list of searched moves
    void add_searched(Move m);
    // Get the searched movelist
    MoveList get_searched_list() const;
    // Return the sizes of the move lists
    template<GenerationType T>
    uint16_t count() const;
    // Return the current see value stored
    Value see_value() const;
    // Set the flag true to skip quiet moves
    void skip_quiets();
    // Read if skipping quiets is true
    bool can_skip_quiets() const;
    // Debugging tool to verify move exists in move lists
    bool is_contained(Move m) const;

private:
    // Add a move to the quiet or capture move list and score it
    template<GenerationType T>
    void add_move(Move m);
    // Generate a set of moves specific to the generation type
    template<GenerationType T>
    void generate();
    // Generate a set of moves specific to the piece
    template<GenerationType T>
    void generate_piece(PieceType pt);
    // Generate pawn moves
    template<GenerationType T>
    void generate_pawns();
    // Generate a generation mask given the position
    void generate_mask();
};

template<GenerationType T>
inline uint16_t Generator::count() const {
    // Evasions have no seperate array so they cannot be counted
    assert(T != EVASIONS);
    // For each case return the according size
    switch (T) {
        case CAPTURES:  return captures.size;
        case QUIETS:    return quiets.size;
        case LEGAL:     return captures.size + quiets.size;
        default:        return 0;
    }
}

inline Value Generator::see_value() const {
    return see;
}

inline void Generator::skip_quiets() {
    skipQuiets = true;
}

inline bool Generator::can_skip_quiets() const {
    return skipQuiets;
}


}

#endif
