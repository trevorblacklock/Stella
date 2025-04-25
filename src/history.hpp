#ifndef HISTORY_H_INCLUDED
#define HISTORY_H_INCLUDED

#include <cstdarg>
#include <array>

#include "types.hpp"
#include "position.hpp"

namespace Stella {

template<typename T, std::size_t Size, std::size_t... Sizes>
class Stats;

template<typename T, std::size_t Size, std::size_t... Sizes>
struct MultiArrayHelper {
    using ChildType = Stats<T, Sizes...>;
};

template<typename T, std::size_t Size>
struct MultiArrayHelper<T, Size> {
    using ChildType = T;
};

template<typename T, std::size_t Size, std::size_t... Sizes>
class Stats {
    using ChildType = typename MultiArrayHelper<T, Size, Sizes...>::ChildType;
    using ArrayType = std::array<ChildType, Size>;
private:
    ArrayType data;
    std::size_t size;
public:
    Stats() : size(Size) { for (auto& i : std::initializer_list<std::size_t>{Sizes...}) size *= i; }
    
    using size_type = typename ArrayType::size_type;

    constexpr auto&       operator[](size_type idx) noexcept { return data[idx]; }
    constexpr const auto& operator[](size_type idx) const noexcept { return data[idx]; }

    void fill(T v) {
        for (auto& ele : data) {
            if constexpr (sizeof...(Sizes) == 0) ele = v;
            else ele.fill(v);
        }
    }
};

struct History {
private:
    // Killer moves, indexed with ply and side to move
    Stats<Move, COLOR_NB, MAX_PLY + 2, 2> killers;
    // Butterfly history, indexed by from and to
    Stats<Value, COLOR_NB, SQ_NB, SQ_NB> butterflyHistory;
    // Continuation history, from Stockfish
    Stats<Value, PIECE_NB, SQ_NB, MAX_PLY + 7> continuationHistory;
    // Capture history, indexed with piece, cap sq and cap piece type
    Stats<Value, PIECE_NB, SQ_NB, PIECE_TYPE_NB> captureHistory;
    // Historic evaluation
    Stats<Value, COLOR_NB, MAX_PLY> evalHistory;
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
