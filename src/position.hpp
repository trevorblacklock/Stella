#ifndef POSITION_H_INCLUDED
#define POSITION_H_INCLUDED

#include <vector>
#include <optional>

#include "bitboard.hpp"
#include "types.hpp"
#include "nn/evaluate.hpp"

namespace Stella {

// PositionInfo structure stores necessary information about the position.
// Most of this information is important in restoring a previous position.
struct PositionInfo {
    Key       key = 0;
    int       castlingRights = 0;
    int       fiftyRule = 0;
    int       pliesFromNull = 0;
    int       repetition = 0;
    Piece     capturedPiece = NO_PIECE;
    Square    epSquare = SQ_NONE;
    Bitboard  checks = 0;
    Bitboard  blockers = 0;
    Bitboard  pinners = 0;
    Value     nonPawnMaterial[COLOR_NB] = {VALUE_ZERO};
    Bitboard  checkSquares[PIECE_TYPE_NB] = {0};
    Move      move = Move::none();
};

// Position class stores everything about a position.
class Position {
private:
    // ** Updated with every move **
    // Bitboards needed to represent the chess board
    Bitboard piecesBB[PIECE_TYPE_NB];
    Bitboard occupiedSideBB[COLOR_NB];
    Bitboard occupiedBB;
    // Piece board to represent which piece is where
    Piece board[SQ_NB];
    // Is position chess960
    bool isChess960;
    // Take extra care to store all castling information for chess960
    Bitboard  castlePath[CASTLE_RIGHT_NB] = {0};
    Square    castleRookSquare[CASTLE_RIGHT_NB] = {SQ_NONE};
    int       castleMask[SQ_NB] = {0};
    // Side to move
    Color sideToMove;
    // Internal move counter
    int moveCount = 0;
    // Vector storing only necessary information of previous positions
    std::vector<PositionInfo> positionHistory;
    // Also store a pointer to the current position
    PositionInfo* current;

    // Store the evaluator
    Network::Evaluator network;

    // Updates the position for checks, pins and blockers
    void update();

    // Set the hash key of the position during initialization
    void set_key();

public:
    // Initialialize the position
    static void init();

    // Evaluate the position using the neural net
    Value evaluate();

    // Constructors using FEN "Forsyth–Edwards Notation",
    // if Chess960 is used then Shredder-FEN or X-FEN will be used over standard FEN.
    // Read more about FEN, X-FEN and Shredder-FEN at the following page:
    // https://www.chessprogramming.org/Forsyth-Edwards_Notation
    Position(const std::string& fen, bool chess960);
    Position(const Position& pos);
    ~Position() = default;
    Position& operator=(const Position& pos);

    // Return a fen string for the given position if possible.
    std::string fen() const;

    // Return the current gamestate.
    inline PositionInfo* gamestate() { return current; }
    inline const PositionInfo* gamestate() const { return current; }

    // Return piece specific bitboards.
    Bitboard pieces(Piece pc) const;
    Bitboard pieces(Color c, PieceType pt) const;
    Bitboard pieces(PieceType pt) const;
    // Return occupied square bitboards.
    Bitboard pieces(Color c) const;
    Bitboard pieces() const;
    // Return combined bitboard for two piecetypes
    Bitboard pieces(PieceType pt1, PieceType pt2) const;
    Bitboard pieces(Color c, PieceType pt1, PieceType pt2) const;

    // Return all the non-pawn material for a side.
    Bitboard non_pawn_material(Color c) const;

    // Return the piece on a given square.
    Piece piece_on(Square s) const;

    // Check if a given square is empty or not.
    bool is_empty(Square s) const;

    // Manipulate the bitboards by moving, setting or removing a piece
    // on all bitboards stored in the position.
    void move_piece(Square from, Square to);
    void set_piece(Piece pc, Square to);
    void pop_piece(Square s);

    // Retrieve information about the current gamestate.
    Key            key() const;
    CastlingRights castling_rights(Color c) const;
    int            fifty_rule() const;
    int            plies_from_null() const;
    bool           repetition() const;
    Piece          captured() const;
    Square         ep_square() const;
    Bitboard       checks() const;
    Bitboard       blockers() const;
    Bitboard       pinners() const;
    Bitboard       check_squares(PieceType pt) const;
    int            move_count() const;
    bool           is_chess960() const;
    Color          side() const;

    // Retrieve information regarding a move.
    Piece piece_moved(Move m) const;
    bool  gives_check(Move m) const;
    bool  is_pseudolegal(Move m) const;
    bool  is_legal(Move m) const;
    bool  is_capture(Move m) const;
    bool  is_promotion(Move m) const;
    bool  is_quiet(Move m) const;

    // Information regarding previous states.
    PositionInfo  previous() const;
    PositionInfo  previous(int count) const;
    Key           previous_key() const;
    Key           previous_key(int count) const;

    // Make a move on the board
    template<bool prefetch, bool lazy=true>
    void do_move(Move m);

    // Undo a move which is assumed to be the previous move resulting in this position.
    template<bool lazy=true>
    void undo_move(Move m);

    // Do or undo a null move which is used in search.
    void do_null();
    void undo_null();

    // Draw detection by threefold or fifty move rule.
    bool is_draw() const;

    // Draw detection by cuckoo tables
    bool has_game_cycled(int ply) const;

    // Change the side from the current to move.
    void change_side();

    // Helper to get the king square for a side
    Square ksq(Color c) const;

    // Set and check for castling rights.
    bool    can_castle(CastlingRights rights) const;
    bool    castling_blocked(CastlingRights rights) const;
    Square  castle_rook_square(CastlingRights rights) const;
    void    set_castling_rights(Color c, Square rook);

    // Static exchange evaluation, used to evaluate the short term value of a capture,
    // read more at https://www.chessprogramming.org/Static_Exchange_Evaluation
    Value see(Move m) const;

    // Return a bitboard for all attackers on a given square
    Bitboard attackers(Square s, std::optional<Bitboard> occupied = std::nullopt) const;

    // Return a bitboard for attacks by a piecetype from the side to move
    Bitboard attacks_by(PieceType pt, Color side) const;

    // Get the gamephase score of the current position
    Value game_phase() const;
};

// Create an operator to send position to a stream
std::ostream& operator<<(std::ostream& os, const Position& pos);

inline Key Position::key() const {
    return current->key;
}

inline PositionInfo Position::previous() const {
    return positionHistory.rbegin()[1];
}

inline PositionInfo Position::previous(int count) const {
    assert(count >= 1);
    return positionHistory.rbegin()[count];
}

inline CastlingRights Position::castling_rights(Color c) const {
    return c & CastlingRights(current->castlingRights);
}

inline int Position::fifty_rule() const {
    return current->fiftyRule;
}

inline int Position::plies_from_null() const {
    return current->pliesFromNull;
}

inline bool Position::repetition() const {
    return current->repetition;
}

inline Piece Position::captured() const {
    return current->capturedPiece;
}

inline Square Position::ep_square() const {
    return current->epSquare;
}

inline Bitboard Position::checks() const {
    return current->checks;
}

inline Bitboard Position::non_pawn_material(Color c) const {
    return current->nonPawnMaterial[c];
}

inline Bitboard Position::blockers() const {
    return current->blockers;
}

inline Bitboard Position::pinners() const {
    return current->pinners;
}

inline Bitboard Position::check_squares(PieceType pt) const {
    return current->checkSquares[pt];
}

inline int Position::move_count() const {
    return moveCount;
}

inline Color Position::side() const {
    return sideToMove;
}

inline bool Position::is_chess960() const {
    return isChess960;
}

inline Bitboard Position::pieces(Piece pc) const {
    return piecesBB[piece_type(pc)] & occupiedSideBB[piece_color(pc)];
}

inline Bitboard Position::pieces(Color c, PieceType pt) const {
    return piecesBB[pt] & occupiedSideBB[c];
}

inline Bitboard Position::pieces(PieceType pt) const {
    return piecesBB[pt];
}

inline Bitboard Position::pieces(Color c) const {
    return occupiedSideBB[c];
}

inline Bitboard Position::pieces() const {
    return occupiedBB;
}

inline void Position::change_side() {
    sideToMove = ~sideToMove;
}

inline Bitboard Position::pieces(PieceType pt1, PieceType pt2) const {
    return pieces(pt1) | pieces(pt2);
}

inline Bitboard Position::pieces(Color c, PieceType pt1, PieceType pt2) const {
    return pieces(pt1, pt2) & pieces(c);
}

inline Piece Position::piece_on(Square s) const {
    assert(is_ok(s));
    return board[s];
}

inline Piece Position::piece_moved(Move m) const {
    assert(m.is_ok());
    return piece_on(m.from());
}

inline bool Position::is_empty(Square s) const {
    assert(is_ok(s));
    return board[s] == NO_PIECE;
}

inline bool Position::can_castle(CastlingRights rights) const {
    assert(castle_rights_ok(rights));
    return rights & CastlingRights(current->castlingRights);
}

inline bool Position::castling_blocked(CastlingRights rights) const {
    assert(castle_rights_ok(rights));
    return pieces() & castlePath[rights];
}

inline Square Position::castle_rook_square(CastlingRights rights) const {
    assert(castle_rights_ok(rights));
    return castleRookSquare[rights];
}

inline bool Position::is_capture(Move m) const {
    assert(m.is_ok());
    return (!is_empty(m.to()) && m.type() != CASTLING) || m.type() == EN_PASSANT;
}

inline bool Position::is_promotion(Move m) const {
    assert(m.is_ok());
    return m.type() == PROMOTION;
}

inline bool Position::is_quiet(Move m) const {
    assert(m.is_ok());
    return is_capture(m) || is_promotion(m);
}

inline Square Position::ksq(Color c) const {
    return lsb(pieces(c, KING));
}

inline void Position::set_piece(Piece pc, Square s) {
    assert(pc != NO_PIECE && is_ok(s));
    board[s] = pc;
    occupiedBB |= piecesBB[piece_type(pc)] |= s;
    occupiedSideBB[piece_color(pc)] |= s;
}

inline void Position::pop_piece(Square s) {
    assert(is_ok(s));
    Piece pc = board[s];
    occupiedBB ^= s;
    occupiedSideBB[piece_color(pc)] ^= s;
    piecesBB[piece_type(pc)] ^= s;
    board[s] = NO_PIECE;
}

inline void Position::move_piece(Square from, Square to) {
    assert(is_ok(from) && is_ok(to));
    Piece pc = board[from];
    Bitboard b = from | to;
    occupiedBB ^= b;
    occupiedSideBB[piece_color(pc)] ^= b;
    piecesBB[piece_type(pc)] ^= b;
    board[from] = NO_PIECE;
    board[to] = pc;
}

}

#endif
