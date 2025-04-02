#ifndef HISTORY_H_INCLUDED
#define HISTORY_H_INCLUDED

#include <iostream>
#include <cstdarg>

#include "types.hpp"
#include "position.hpp"

namespace Stella {

template<typename T>
struct Stats {
private:
    T* data;
    int num;
    size_t size;
    std::vector<int> coeff;

public:
    Stats(int dim, ...);
    Stats(const Stats<T>& s);
    ~Stats() { delete data; };

    void reset(T v) { std::fill(data, data + size, v); };

    Stats<T>& operator=(const Stats<T>& s);
    T operator()(int idx, ...) const;
    T& operator()(int idx, ...);
};

struct History {
private:
    // Killer moves, indexed with ply and side to move
    Stats<Move> killers = Stats<Move>(3, COLOR_NB, MAX_PLY + 2, 2);
    // Butterfly history, indexed by from and to
    Stats<Value> butterflyHistory = Stats<Value>(3, COLOR_NB, SQ_NB, SQ_NB);
    // Continuation history, from Stockfish
    Stats<Value> continuationHistory = Stats<Value>(3, MAX_PLY + 7, PIECE_NB, SQ_NB);
    // Capture history, indexed with piece, cap sq and cap piece type
    Stats<Value> captureHistory = Stats<Value>(3, PIECE_NB, SQ_NB, PIECE_TYPE_NB);
    // Historic evaluation
    Stats<Value> evalHistory = Stats<Value>(2, COLOR_NB, MAX_PLY);
public:
    History() { clear(); };
    // Clear the structure
    void clear();
    // Reset grandchildren of current killer moves
    void clear_killers_grandchildren(Color side, int ply);
    // Function to return the history
    Value get_history(Position* pos, Move m, int ply) const;
    // Functions to get specific histories
    Move get_killer(Color side, int ply, int id) const;
    Value get_butterfly(Color side, Move m) const;
    Value get_capture(Piece pc, Square to, PieceType pt) const;
    Value get_continuation(Piece pc, Square sq, int ply) const;
    Value get_eval(Color side, int ply) const;
    // Functions to set values
    void set_killer(Color side, Move m, int ply);
    void set_butterfly(Color side, Move m, Value v);
    void set_capture(Piece pc, Square to, PieceType pt, Value v);
    void set_continuation(Piece pc, Square sq, int ply, Value v);
    void set_eval(Color side, int ply, Value v);
    // Find if position is improving based on historic eval
    bool is_improving(Color side, int ply, Value v) const;
    // Check if move is a killer
    bool is_killer(Color side, Move m, int ply) const;
    // Functions to update values
    void update_butterfly(Color side, Move m, Value b);
    void update_capture(Piece pc, Square to, PieceType pt, Value b);
    void update_continuation(Piece pc, Square sq, int ply, Value b);
};

}

#endif
