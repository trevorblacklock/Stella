#include <iostream>
#include <cassert>
#include <string>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <algorithm>

#include "position.hpp"
#include "bitboard.hpp"
#include "types.hpp"
#include "misc.hpp"

namespace Stella {

namespace Zobrist {

// Store all zobrist hash keys here
Key pieces[PIECE_NB][SQ_NB];
Key enpassant[FILE_NB];
Key castling[CASTLE_RIGHT_NB];
Key side;

}

// Initalize hash keys
void Position::init() {
    // Pseudo random number generator with the given seed
    Random rng(534895);

    // Write keys for every piece
    for (PieceType pt = PAWN; pt <= KING; ++pt)
        for (Color c : {WHITE, BLACK})
            for (Square s = A1; s <= H8; ++s)
                Zobrist::pieces[make_piece(c, pt)][s] = rng.random<Key>();

    // Write keys for enpassant squares
    for (File f = FILE_A; f<= FILE_H; ++f)
        Zobrist::enpassant[f] = rng.random<Key>();

    // Write keys for castling
    for (CastlingRights rights = NO_CASTLE; rights <= ANY_CASTLE; ++rights)
        Zobrist::castling[rights] = rng.random<Key>();

    // Write keys for side
    Zobrist::side = rng.random<Key>();
}

std::ostream& operator<<(std::ostream& os, const Position& pos) {
    // Write a string to the stream
    os << "+-----------------+\n";
    // Loop through all the squares on the board
    for (Rank r = RANK_8; r >= RANK_1; --r) {
        os << "|";
        for (File f = FILE_A; f <= FILE_H; ++f) {
        Piece pc = pos.piece_on(make_square(r, f));
        os << " " << pieceChar[pc];
        }
        os << " | " << 1 + r << "\n";
    }
    os << "+-----------------+\n";
    os << "  a b c d e f g h\n";

    // Write important position information like FEN, hash key and check squares
    os << "\nFen: " << pos.fen() << "\nKey: 0x" << std::hex << std::uppercase << std::setfill('0')
        << std::setw(16) << pos.key() << std::setfill(' ') << std::dec << "\nCheckers: ";

    for (Bitboard b = pos.checks(); b;) os << square(pop_lsb(b)) << " ";

    return os;
}

Position::Position(const std::string& fen, bool chess960) {
    // Set everything to zero before reading in the FEN string
    std::fill(std::begin(this->piecesBB), std::end(this->piecesBB), 0);
    std::fill(std::begin(this->occupiedSideBB), std::end(this->occupiedSideBB), 0);
    std::fill(std::begin(this->board), std::end(this->board), NO_PIECE);
    this->occupiedBB = 0;

    // FEN strings begin from A8
    Square s = A8;
    // Keep track of the piece
    Piece pc;
    // Keep track of the current token read in by the stream
    char token;
    // Create a string stream to read in the FEN string
    std::istringstream ss(fen);

    // Initialize an empty state
    PositionInfo pi{};
    // Write the empty state into the position history and set it as the current state
    this->positionHistory.push_back(pi);
    this->current = &this->positionHistory.back();

    // Set the stream to not skip whitespace
    ss >> std::noskipws;

    // 1. Loop through all the pieces (until there is a space)
    while ((ss >> token) && !std::isspace(token)) {
        // Check if the token is a number
        if (std::isdigit(token)) s += Direction(token - '0');
        // Check if the token is a slash for new rank
        else if (token == '/') s += SOUTH_SOUTH;
        // Check for token identifying a piece to place
        else if ((pc = Piece(pieceChar.find(token))) != std::string::npos) {
        // For each set piece adjust the hash key
        current->key ^= Zobrist::pieces[pc][s];
        set_piece(pc, s);
        ++s;
        // Also add the value of the piece if it is not a pawn or king
        if (piece_type(pc) != PAWN && piece_type(pc) != KING)
            current->nonPawnMaterial[piece_color(pc)] += piece_value(pc);
        }
  }

    // 2. Once a space is reached next information should be side to move
    ss >> token;
    this->sideToMove = (token == 'w' ? WHITE : BLACK);
    ss >> token;
    // Adjust hash key for side to move
    current->key ^= Zobrist::side;

    // 3. Castling Rights, if Chess960 is selected then Shredder-FEN or X-FEN must be
    // used to show the castling rights of each side instead of the standard FEN format.
    while ((ss >> token) && !std::isspace(token)) {
        // Keep track of the castling sides color and rook square
        Square rookSquare;
        Color castleColor = std::islower(token) ? BLACK : WHITE;
        Piece rook = make_piece(castleColor, ROOK);

        // Transfer the token to uppercase for fewest cases since all info is already gotten
        token = std::toupper(token);
        // Run through all castle cases
        if (token == 'K')
            for (rookSquare = relative_square(castleColor, H1); piece_on(rookSquare) != rook; --rookSquare) {}
        else if (token == 'Q')
            for (rookSquare = relative_square(castleColor, A1); piece_on(rookSquare) != rook; ++rookSquare) {}
        else if (token >= 'A' && token <= 'H')
            rookSquare = make_square(relative_rank(castleColor, RANK_1), File(token - 'A'));
        else continue;

        // Set the castling rights now for the given rook square
        set_castling_rights(castleColor, rookSquare);
  }

    // Update the hash key with the castling rights
    current->key ^= Zobrist::castling[current->castlingRights];

    // 4. En passant square, ignore if the square is invalid
    ss >> token;
    File f = File(token - 'a');
    ss >> token;
    Rank r = Rank(token - '1');

    // Check if rank and file are valid
    if (f >= FILE_A && f <= FILE_H && r >= RANK_1 && r <= RANK_8
        && r == (this->sideToMove == WHITE ? RANK_6 : RANK_3)) {

        // Write the square now
        current->epSquare = make_square(r, f);
        // Adjust hash key for enpassant square
        current->key ^= Zobrist::enpassant[f];
    }
    else current->epSquare = SQ_NONE;

    // 5. Halfmove clock
    ss >> std::skipws >> current->fiftyRule >> this->moveCount;

    // Convert the fullmove number to halfmove
    this->moveCount = std::max(2 * (this->moveCount - 1), 0) + (this->sideToMove == BLACK);
    // Set if position is Chess960
    this->isChess960 = chess960;

    // Finally update the game state
    this->update();
}

// Assigns a position to a new one
Position& Position::operator=(const Position& pos) {
    // Copy the bitboards
    std::copy(std::begin(pos.piecesBB), std::end(pos.piecesBB), this->piecesBB);
    std::copy(std::begin(pos.occupiedSideBB), std::end(pos.occupiedSideBB), this->occupiedSideBB);
    std::copy(std::begin(pos.board), std::end(pos.board), this->board);
    this->occupiedBB = pos.occupiedBB;

    // Copy game information
    this->sideToMove = pos.sideToMove;
    this->moveCount = pos.moveCount;
    this->isChess960 = pos.isChess960;

    // Copy the castling rights
    std::copy(std::begin(pos.castleMask), std::end(pos.castleMask), this->castleMask);
    std::copy(std::begin(pos.castleRookSquare), std::end(pos.castleRookSquare), this->castleRookSquare);
    std::copy(std::begin(pos.castlePath), std::end(pos.castlePath), this->castlePath);

    // Copy the position history
    this->positionHistory.clear();
    for (auto &entry : pos.positionHistory)
        this->positionHistory.push_back(entry);

    this->current = &this->positionHistory.back();

    // Update the state info
    this->update();

    return *this;
}

// Copies a position to a new one
Position::Position(const Position& pos) {
    *this = pos;
}

std::string Position::fen() const {
    // Create a stream to write the FEN to
    std::ostringstream ss;
    // Keep track of the number of empty squares
    int offset = 0;

    // Loop through all ranks and files
    for (Rank r = RANK_8; r >= RANK_1; --r) {
        for (File f = FILE_A; f <= FILE_H; ++f) {
        // Check the squares on this rank to see if they are empty
        for (offset = 0; f <= FILE_H && is_empty(make_square(r, f)); ++f) ++offset;
        // If there are empty squares right to the FEN
        if (offset) ss << offset;
        // If end of board is not reached must be a non empty square, so write the piece
        if (f <= FILE_H) ss << pieceChar[piece_on(make_square(r, f))];
        }
        // For each rank passed add a slash except last one
        if (r != RANK_1) ss << '/';
    }

    // Write the side to move
    ss << (this->sideToMove == WHITE ? " w " : " b ");

    // Write the castling rights
    if (can_castle(WHITE_KING))
        ss << (this->isChess960 ? char('A' + file_of(castle_rook_square(WHITE_KING))) : 'K');
    if (can_castle(WHITE_QUEEN))
        ss << (this->isChess960 ? char('A' + file_of(castle_rook_square(WHITE_QUEEN))) : 'Q');
    if (can_castle(BLACK_KING))
        ss << (this->isChess960 ? char('a' + file_of(castle_rook_square(BLACK_KING))) : 'k');
    if (can_castle(BLACK_QUEEN))
        ss << (this->isChess960 ? char('a' + file_of(castle_rook_square(BLACK_QUEEN))) : 'q');
    if (!can_castle(ANY_CASTLE))
        ss << '-';

    // Write the enpassant square
    if (ep_square() == SQ_NONE)
        ss << " - ";
    else
        ss << " " << square(ep_square()) << " ";

    // Write halfmove and fullmove clock
    ss << current->fiftyRule << " " << 1 + (this->moveCount - (this->sideToMove == BLACK)) / 2;

    return ss.str();
}

// Function to set the castling rights for a given side and rook square
void Position::set_castling_rights(Color c, Square rookSquare) {
    // Get the king square for the side
    Square kingFrom = ksq(c);
    CastlingRights rights = c & (kingFrom < rookSquare ? KING_SIDE : QUEEN_SIDE);

    // Update the castling rights
    current->castlingRights |= rights;
    this->castleMask[kingFrom] |= rights;
    this->castleMask[rookSquare] |= rights;
    this->castleRookSquare[rights] = rookSquare;

    Square kingTo = relative_square(c, rights & KING_SIDE ? G1 : C1);
    Square rookTo = relative_square(c, rights & KING_SIDE ? F1 : D1);

    this->castlePath[rights] = (between_bb(rookSquare, rookTo) | between_bb(kingFrom, kingTo)) & ~(kingFrom | rookSquare);
}

// Updates pins and blockers for legality
void Position::update() {
    // Get the side to move
    Color us = this->sideToMove;
    Color them = ~us;

    // Store the available blockers for the king and pinning pieces
    Bitboard pinners = 0, blockers = 0;

    // Get the king squares
    Square ks = ksq(us);
    Square eks = ksq(them);

    // Get sliders from opponent
    Bitboard lateralSliders = pieces(them, ROOK) | pieces(them, QUEEN);
    Bitboard diagonalSliders = pieces(them, BISHOP) | pieces(them, QUEEN);

    // Find the sliders that threaten the friendly king
    Bitboard sliderAttacks = (attacks_bb(ks, ROOK, pieces(them)) & lateralSliders)
                            | (attacks_bb(ks, BISHOP, pieces(them)) & diagonalSliders);

    // Find the pieces that check the king and cannot be blocked
    Bitboard checkers = (pawn_attacks(us, ks) & pieces(them, PAWN))
                        | (attacks_bb(ks, KNIGHT) & pieces(them, KNIGHT));

    // Loop through all slider attacks and update check, block and pin information
    while (sliderAttacks) {
        // Get the square of the attacker
        Square s = pop_lsb(sliderAttacks);
        // Get the friendly pieces between the king and attacker
        Bitboard between = between_bb(ks, s) & pieces(us);

        // Count the amount of blockers between the king and attacker
        uint8_t blockerCount = popcount(between);

        // If the blocker number is zero than the attacker is checking the king,
        // otherwise the blocker is pinning the pieces if there is only one piece
        if (!blockerCount) checkers |= s;
        else if (blockerCount == 1) {
            pinners |= s;
            blockers |= lsb(between);
        }
    }

    // Write the check and other information to the state
    current->pinners = pinners;
    current->blockers = blockers;
    current->checks = checkers;
    current->checkSquares[PAWN] = pawn_attacks(them, eks);
    current->checkSquares[KNIGHT] = attacks_bb(eks, KNIGHT);
    current->checkSquares[BISHOP] = attacks_bb(eks, BISHOP, this->occupiedBB);
    current->checkSquares[ROOK] = attacks_bb(eks, ROOK, this->occupiedBB);
    current->checkSquares[QUEEN] = current->checkSquares[ROOK] | current->checkSquares[BISHOP];
}

Bitboard Position::attackers(Square s, std::optional<Bitboard> occupied) const {
  // Check if an occupied bitboard has been provided otherwise use the position
  Bitboard o = (occupied == std::nullopt) ? this->occupiedBB : occupied.value();

  // Return all attacks acting on this square using the occupied board
  return (pawn_attacks(WHITE, s) & pieces(B_PAWN))
       | (pawn_attacks(BLACK, s) & pieces(W_PAWN))
       | (attacks_bb(s, KNIGHT) & pieces(KNIGHT))
       | (attacks_bb(s, BISHOP, o) & pieces(BISHOP, QUEEN))
       | (attacks_bb(s, ROOK, o) & pieces(ROOK, QUEEN))
       | (attacks_bb(s, KING) & pieces(KING));
}

Bitboard Position::attacks_by(PieceType pt) const {
    // Get the side to move
    Color us = side();
    // Create an attacker board
    Bitboard attackers = pieces(us, pt);
    // Check for piece being a pawn
    if (pt == PAWN) return pawn_attacks_bb(us, attackers);
    // For rest of the pieces
    else {
        // Loop through all pieces and add them to threats
        Bitboard threat = 0;
        while (attackers) threat |= attacks_bb(pop_lsb(attackers), pt, pieces());
        return threat;
    }
}

bool Position::is_legal(Move m) const {
    // Make sure move is ok
    assert(m.is_ok());

    // Get the side to move
    Color us = side();
    Color them = ~us;

    // Get the king square
    Square ks = ksq(us);

    // Get the from and to square of the move
    Square from = m.from();
    Square to = m.to();

    // Get the movetype
    MoveType type = m.type();

    // Get the piece moving
    Piece pc = piece_moved(m);

    // Check first for enpassant moves
    if (type == EN_PASSANT) {
        // Get the capture square for this move
        Square captureSquare = to - pawn_push(us);
        // Update the occupied board as if the capture was made
        Bitboard occupied = (pieces() ^ from ^ captureSquare) | to;

        // Make sure enpassant capture doesn't break any big rules
        assert(to == ep_square());
        assert(pc == make_piece(us, PAWN));
        assert(piece_on(captureSquare) == make_piece(them, PAWN));
        assert(piece_on(to) == NO_PIECE);

        // Return that the move does not reveal any attacks with the new occupied board
        return !(attacks_bb(ks, ROOK, occupied) & (pieces(them, ROOK) | pieces(them, QUEEN)))
            && !(attacks_bb(ks, BISHOP, occupied) & (pieces(them, BISHOP) | pieces(them, QUEEN)));
    }

    // Check castling moves
    if (type == CASTLING) {
        // Cannot castle when in check
        if (checks()) return false;
        // Get the direction the rook is in
        Direction d = from < to ? EAST : WEST;
        // Get the final king square
        Square kingTo = (to > from) ? relative_square(us, G1) : relative_square(us, C1);
        // Check that there are no attacks between the king and rook
        for (Square s = from + d; s != kingTo + d; s += d)
        if (attackers(s, pieces()) & pieces(them)) return false;
    }

    // If moving piece is a king then make sure the square is not under attack
    else if (piece_type(pc) == KING) {
        // Special case for castling moves since to square is coded as rook square
        return !(attackers(to, pieces() ^ from) & pieces(them));
    }

    // Make sure a moving piece does not reveal an attack on the friendly king,
    // if friendly king is also in check then a pinned piece cannot be moved
    if (blockers() & from) return lies_along(from, to, ks) && !checks();

    // If the king is being attacked by only one piece then check if this move can block
    if (popcount(checks()) == 1) return (between_bb(ks, lsb(checks())) & to);

    // If the king is being attacked by two pieces then only king has a move,
    // since king moves are already handled above, by this point the move must not be legal
    if (popcount(checks()) > 1) return false;

    // If nothing has been flagged yet then the move must be legal
    return true;
}

// Checks that a move can be made without considering attacks on the king since
// moves from the transposition table can be corrupted or can be misindexed and
// not be possible moves in the current position.
bool Position::is_pseudolegal(Move m) const {
    // If move has same from and to square then it is not possible
    if (!m.is_ok()) return false;

    // Get the from and to square
    Square from = m.from();
    Square to = m.to();

    // Get the piece moved
    Piece pc = piece_moved(m);

    // Get the type of move
    MoveType type = m.type();

    // Get the side to move
    Color us = side();
    Color them = ~us;

    // Get the captured piece
    Piece captured = type == EN_PASSANT ? make_piece(them, PAWN) : piece_on(to);

    // Make sure moving our own piece
    if (piece_color(pc) != us) return false;

    // Make sure capturing one of their pieces
    if (captured != NO_PIECE && piece_color(captured) != them) return false;

    // Make sure not capturing a king
    if (piece_type(captured) == KING) return false;

    // Make sure enpassant or promotion moves are only with pawns
    if ((type == PROMOTION || type == EN_PASSANT) && piece_type(pc) != PAWN) return false;

    // Make sure a castling move is only done by king
    if (type == CASTLING && piece_type(pc) != KING) return false;

    // Make sure castling is possible
    if (type == CASTLING) {
        CastlingRights rights = us & (from < to ? KING_SIDE : QUEEN_SIDE);
        if (!can_castle(rights)) return false;
        if (castling_blocked(rights)) return false;
    }

    // Handle pawn moves
    if (piece_type(pc) == PAWN) {
        // Check that non enpassant moves only move in a way a pawn can,
        if (type != EN_PASSANT) {
        // Make sure the pawn lands on a legal square
        if (!(to & ((from + pawn_push(us)) | (from + 2 * pawn_push(us)) | pawn_attacks(us, from)))) return false;

        // For single pawn pushes
        if (to & (from + pawn_push(us)) && !is_empty(to)) return false;

        // For double pawn pushes
        if (to & (from + 2 * pawn_push(us)) && !is_empty(from + pawn_push(us)) && !is_empty(to)) return false;

        // A double pawn push must land on the fourth rank and start on the second
        if (to & (from + 2 * pawn_push(us)) && relative_rank(us, from) != RANK_2 && relative_rank(us, to) != RANK_4) return false;

        // Only allow captures if the pawn moves diagonally
        if (captured != NO_PIECE && !(pawn_attacks(us, from) & to)) return false;

        // Only allow diagonal movement if there is a capture
        if ((pawn_attacks(us, from) & to) && captured == NO_PIECE) return false;
        }

        // Check that enpassant moves land on the correct square
        if (type == EN_PASSANT && ep_square() != to) return false;

        // Check that enpassant moves land on the correct rank
        if (type == EN_PASSANT && (piece_on(to - pawn_push(us)) != make_piece(them, PAWN)
        || relative_rank(us, to) != RANK_6)) return false;

        // Check that promotions land on the 1st or 8th rank and begin on the 2nd or 7th
        if (type == PROMOTION && !(rank_of(to) == relative_rank(us, RANK_8) && rank_of(from) == relative_rank(us, RANK_7)))
            return false;
    }

    // Check that other pieces move properly
    else {
        // Check that pieces land on possible squares
        if (!(to & attacks_bb(from, piece_type(pc)))) return false;

        // Check that there are no pieces between the move for sliders
        if (piece_type(pc) != KNIGHT && piece_type(pc) != KING && (between_bb(from, to) ^ to) & pieces()) return false;
    }

    return true;
}

template<bool prefetch>
void Position::do_move(Move m) {
    // Assert move is ok
    assert(m.is_ok());
    // Store the current position as the previous ensuring to dereference from vector
    PositionInfo previous = *current;
    // Create a new state to push onto the end of the vector
    positionHistory.emplace_back(PositionInfo {});
    // Create a pointer to the new position object
    current = &positionHistory.back();

    // Update the new state with the old one
    current->key = previous.key;
    current->castlingRights = previous.castlingRights;
    current->fiftyRule = previous.fiftyRule;
    current->pliesFromNull = previous.pliesFromNull;
    std::copy(std::begin(previous.nonPawnMaterial), std::end(previous.nonPawnMaterial), std::begin(current->nonPawnMaterial));

    // Get the from and to square from the move
    const Square from = m.from();
    const Square to = m.to();

    // Get the piece moved
    const Piece pc = piece_on(from);

    // Get the side to move
    const Color us = sideToMove;
    const Color them = ~us;

    // Get the type of move
    const MoveType type = m.type();

    // Get the captured piece if any
    Piece captured = (type == EN_PASSANT) ? make_piece(them, PAWN) : piece_on(to);

    // Basic assertions
    assert(pc != NO_PIECE);
    assert(piece_color(pc) == us);

    // Increment internal counters
    ++moveCount;
    ++current->fiftyRule;
    ++current->pliesFromNull;

    // Firstly deal with castling moves, a castling move is coded as from: ksq to: rooksq,
    // so essentially is a type of capture, need to be careful with that.
    if (type == CASTLING) {
        // Assert the king being moved
        assert(piece_type(pc) == KING);
        // Assert the rook is ours
        assert(piece_on(to) == make_piece(us, ROOK));
        // Find out what type of castling is occuring and adjust the to square for the king
        Square kingFrom = from;
        Square kingTo = to > from ? relative_square(us, G1) : relative_square(us, C1);
        // Now find the square for the rook
        Square rookFrom = to;
        Square rookTo = to > from ? relative_square(us, F1) : relative_square(us, D1);

        // Remove pieces seperately and add them after since squares can overlap in Chess960
        pop_piece(kingFrom);
        pop_piece(rookFrom);

        // Ensure pieces are removed
        assert(piece_on(kingFrom) == NO_PIECE && piece_on(rookFrom) == NO_PIECE);

        // Add the pieces back
        set_piece(make_piece(us, KING), kingTo);
        set_piece(make_piece(us, ROOK), rookTo);

        // Update the zobrist key
        current->key ^= Zobrist::pieces[pc][kingFrom] ^ Zobrist::pieces[pc][kingTo]
                    ^ Zobrist::pieces[piece_on(to)][rookFrom] ^ Zobrist::pieces[piece_on(to)][rookTo];

        // Set the captured piece to none
        captured = NO_PIECE;

        // Update the castling rights and zobrist key, first remove old castling rights
        current->key ^= Zobrist::castling[current->castlingRights];
        // Update castling rights now
        current->castlingRights &= ~(castleMask[kingFrom] | castleMask[kingTo]);
        // Add new castle rights
        current->key ^= Zobrist::castling[current->castlingRights];
    }

    // Now deal with captures, this includes enpassant moves
    if (captured != NO_PIECE) {
        // Get the capture square
        Square captureSq = to;

        // Handle enpassant moves
        if (type == EN_PASSANT) {
            // Update the capture square
            captureSq -= pawn_push(us);
            // Check some basic assertions
            assert(pc == make_piece(us, PAWN));
            assert(piece_on(to) == NO_PIECE);
            assert(to == previous.epSquare);
            assert(relative_rank(us, to) == RANK_6);
        }

        // Remove the piece from the capture square, piece will be moved later
        pop_piece(captureSq);
        // Reset the fifty move rule
        current->fiftyRule = 0;
        // Update the zobrist key
        current->key ^= Zobrist::pieces[captured][captureSq];
        // Update non pawn material
        if (piece_type(captured) != PAWN) current->nonPawnMaterial[them] -= piece_value(captured);

    }

    // Regardless of if an enpassant move is made the square is reset and the zobrist key must be updated
    if (previous.epSquare != SQ_NONE)
        current->key ^= Zobrist::enpassant[file_of(previous.epSquare)];

    // Update the castling rights for other moves
    if (type != CASTLING && current->castlingRights && (castleMask[from] | castleMask[to])) {
        // Update the castling rights and zobrist key, first remove old castling rights
        current->key ^= Zobrist::castling[current->castlingRights];
        // Update castling rights now
        current->castlingRights &= ~(castleMask[from] | castleMask[to]);
        // Add new castle rights
        current->key ^= Zobrist::castling[current->castlingRights];
    }

    // For non castling moves actually move the piece now and update the zobrist key
    if (type != CASTLING) {
        move_piece(from, to);
        current->key ^= Zobrist::pieces[pc][from] ^ Zobrist::pieces[pc][to];
    }

    // Handle pawn moves, this includes promotions
    if (piece_type(pc) == PAWN) {
        // If a double pawn push was made set the enpassant square and update the zobrist key
        if (abs(to - from) == NORTH + NORTH) {
            current->epSquare = to - pawn_push(us);
            current->key ^= Zobrist::enpassant[file_of(current->epSquare)];
        }

        // Handle promotions which cannot occur on double pushes
        else if (type == PROMOTION) {
            // Get the promotion piece
            Piece promotion = make_piece(us, m.promotion());
            // Check some assertions
            assert(relative_rank(us, to) == RANK_8);
            assert(piece_type(promotion) >= KNIGHT && piece_type(promotion) <= QUEEN);

            // Remove the pawn and add back the promoted piece
            pop_piece(to);
            set_piece(promotion, to);

            // Update the zobrist key now to account for new piece on to square
            current->key ^= Zobrist::pieces[pc][to] ^ Zobrist::pieces[promotion][to];

            // Update non pawn material with the promoted piece
            current->nonPawnMaterial[us] += piece_value(promotion);
        }

        // For all pawn moves reset the fifty move rule
        current->fiftyRule = 0;
    }

    // Set the captured piece
    current->capturedPiece = captured;

    // Change the side to move
    change_side();

    // Update state information
    update();

    // Ensure reptition is false
    current->repetition = false;
    // Check how far can go back
    size_t inc = std::min(current->fiftyRule, current->pliesFromNull);
    // Only update repetitions if inc is large enough
    if (inc >= 4) {
        // Update the repetitions for the current position
        size_t end   = std::max(size_t(0), positionHistory.size() - 5);
        size_t start = std::max(size_t(0), positionHistory.size() - 1 - inc);
        // Loop through possible repeating positions, don't bother if fifty rule is small enough
        for (size_t idx = start; idx >= end; idx += 2) {
            if (positionHistory.at(idx).key == current->key) {
                current->repetition = true;
                break;
            }
        }
    }

}

// Instantiate the templates
template void Position::do_move<true>(Move m);
template void Position::do_move<false>(Move m);

void Position::undo_move(Move m) {
    // Check that the move is ok
    assert(m.is_ok());

    // Check that there is at least 2 stored states
    assert(positionHistory.size() > 1);

    --moveCount;

    // Change side to move
    change_side();

    // Get the new side to move
    const Color us = side();

    // Get the from and to squares
    const Square from = m.from();
    const Square to = m.to();

    // Get the moved piece
    Piece pc = piece_on(to);

    // Get the type of move
    const MoveType type = m.type();

    // Check that the from square is empty unless castling in Chess960 since rook and king can overlap
    assert(is_empty(from) || type == CASTLING);
    assert(piece_type(current->capturedPiece) != KING);

    // Handle promotions
    if (type == PROMOTION) {
        // Check that a promotion is not breaking any rules
        assert(relative_rank(us, to) == RANK_8);
        assert(piece_type(pc) == m.promotion());
        assert(piece_type(pc) >= KNIGHT && piece_type(pc) <= QUEEN);

        // Remove the piece and place the pawn back
        pop_piece(to);
        // Set the new piece to be a pawn
        pc = make_piece(us, PAWN);
        // Replace old promoted piece with a pawn, will be moved later
        set_piece(pc, to);
    }

    // Handle castling moves
    if (type == CASTLING) {
        // Find out what type of castling is occuring and adjust the to square for the king
        Square kingFrom = to > from ? relative_square(us, G1) : relative_square(us, C1);
        Square kingTo = from;
        // Assert the king being moved
        pc = piece_on(kingFrom);
        assert(piece_type(pc) == KING);
        // Now find the square for the rook
        Square rookFrom = to > from ? relative_square(us, F1) : relative_square(us, D1);
        Square rookTo = to;

        // Remove pieces seperately and add them after since squares can overlap in Chess960
        pop_piece(kingFrom);
        pop_piece(rookFrom);

        // Ensure pieces are removed
        assert(piece_on(kingFrom) == NO_PIECE && piece_on(rookFrom) == NO_PIECE);

        // Add the pieces back
        set_piece(make_piece(us, KING), kingTo);
        set_piece(make_piece(us, ROOK), rookTo);
    }

    // For any other move
    else {
        // Move the piece back to the from square
        move_piece(to, from);

        // Handle captures now
        if (current->capturedPiece != NO_PIECE) {
        // Get the capture square
        Square captureSq = to;

        // Handle enpassant moves
        if (type == EN_PASSANT) {
            // Update the capture square
            captureSq -= pawn_push(us);
            // Check some basic assertions
            assert(pc == make_piece(us, PAWN));
            assert(piece_on(to) == NO_PIECE);
            assert(to == positionHistory.end()[-2].epSquare);
            assert(relative_rank(us, to) == RANK_6);
        }

        // Place piece back
        set_piece(current->capturedPiece, captureSq);
        }
    }

    // Remove the state and adjust the current pointer
    current = &positionHistory.end()[-2];
    positionHistory.pop_back();
}

// Tests if a position draws by repetition or by 50-move rule
bool Position::is_draw() const {
    // Fifty move rule on draws if no checks
    if (current->fiftyRule > 99 && !current->checks) return true;

    // Return a draw if a position repeats once earlier
    return current->repetition;
}

Value Position::game_phase() const {
        // Get the occupied board excluding pawns
        Bitboard occupied = pieces(PAWN) ^ pieces();

        // Loop through all occupied pieces
        Square sq;
        Value v = 0;

        while (occupied) {
            sq = pop_lsb(occupied);
            // Get the piece value
            Piece pc = piece_on(sq);
            // Add all the values up
            v += piece_value(pc);
        }

        // Return the score at the end
        Value clamped = std::clamp(v, (Value)ENDGAME_CAP, (Value)MIDGAME_CAP);
        Value phase = ((clamped - ENDGAME_CAP) * 128) / (MIDGAME_CAP - ENDGAME_CAP);
        return phase;
}

Value Position::see(Move m) const {
    // Check the move is not a special or invalid case
    assert(m.is_ok());
    if (m.type() != NORMAL) return 0;

    // Get move information
    Square from = m.from();
    Square to = m.to();
    PieceType ptFr = piece_type(piece_on(from));
    PieceType ptTo = piece_type(piece_on(to));
    Color us = side();

    // Ensure move is a capture
    if (ptTo == NO_PIECE_TYPE) return 0;

    // Store recursive scores, maximum of 32 for max pieces on board
    Value score[32];
    Depth depth = 0;
    score[0] = piece_value(ptTo).mid;

    // Get bitboards and remove the current piece
    Bitboard occupied = pieces() ^ from ^ to;
    Bitboard rooks = pieces(ROOK);
    Bitboard bishops = pieces(BISHOP);
    Bitboard queens = pieces(QUEEN);

    // Make horizontal and diagonal sliders
    Bitboard horizontal = (rooks | queens) & ~square_bb(from);
    Bitboard diagonal = (bishops | queens) & ~square_bb(from);

    // Find attackers of the to square
    Bitboard attacks = attackers(to, occupied);

    // Loop through all captures until there are none left
    while (true) {
        // Increment the depth
        depth++;
        // Update the attack board and find active attackers
        attacks &= occupied;
        Bitboard active = attacks & pieces(us);

        // If no active attackers then break the loop
        if (!active) break;

        // Now find the least valuable piece that can initiate the capture
        PieceType pt;
        for (pt = PAWN; pt <= KING; ++pt)
            if (pieces(us, pt) & active) break;

        // Adjust the score based on the move made
        score[depth] = piece_value(ptFr).mid - score[depth - 1];

        // Break early if double negative
        if (std::max(-score[depth - 1], score[depth]) < 0) break;

        // Update the piece
        ptFr = pt;

        // Now make the move on the occupied board and update the attacks
        Square s = lsb(pieces(us, pt) & attacks);
        occupied ^= s;

        // Update the attacks
        if (pt == PAWN || pt == BISHOP || pt == QUEEN)
            attacks |= attacks_bb(to, BISHOP, occupied) & diagonal;
        else if (pt == ROOK || pt == QUEEN)
            attacks |= attacks_bb(to, ROOK, occupied) & horizontal;

        // Change the side
        us = ~us;
    }

    // Choose the best value to represent the chain of captures
    while (--depth) score[depth - 1] = -std::max(-score[depth - 1], score[depth]);

    return score[0];
}

}
