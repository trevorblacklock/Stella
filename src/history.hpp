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
    Stats(int dim, ...) {
        assert(dim > 0);
        num = dim;

        // Store the shape temporarily to compute the coefficients to map Z^n to Z
        std::vector<int> shape;

        // Create variadic arguments
        std::va_list args;
        va_start(args, dim);
        int curr;
        size = 1;

        // Loop through arguments
        for (int i = 0; i < dim; ++i) {
            curr = va_arg(args, int);
            assert(curr > 0);
            size *= curr;
            shape.push_back(curr);
        }

        // Allocate memory to data pointer
        data = new T[size];

        // Compute coefficients of map
        for (int i1 = 1; i1 < dim; ++i1) {
            curr = 1;
            for (int i2 = i1; i2 < dim; ++i2) {
                curr *= shape[i2];
            }
            coeff.push_back(curr);
        }
        // Final index should be one
        coeff.push_back(1);
    }

    ~Stats() { delete data; };

    void reset(T v) { std::fill(data, data + size, v); };

    T operator()(int idx, ...) const {
        assert(idx >= 0);

        // Multiply first index by first coefficient
        idx *= coeff[0];

        // Create variadic arguments
        std::va_list args;
        va_start(args, idx);
        int curr;

        // Loop through arguments and apply coefficients
        for (int i = 1; i < num; ++i) {
            curr = va_arg(args, int);
            assert(curr >= 0);
            idx += curr * coeff[i];
        }

        // Return the data at the computed location
        return data[idx];
    }

    T& operator()(int idx, ...) {
        assert(idx >= 0);

        // Multiply first index by first coefficient
        idx *= coeff[0];

        // Create variadic arguments
        std::va_list args;
        va_start(args, idx);
        int curr;

        // Loop through arguments and apply coefficients
        for (int i = 1; i < num; ++i) {
            curr = va_arg(args, int);
            assert(curr >= 0);
            idx += curr * coeff[i];
        }

        // Return the data at the computed location
        return data[idx];
    }
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
