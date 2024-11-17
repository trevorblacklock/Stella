#ifndef EVALUATE_H_INCLUDED
#define EVALUATE_H_INCLUDED

#include "accumulator.hpp"

#include <memory>
#include <vector>

namespace Stella::Network {

// Main evaluator class to handle accumulator updates and evaluations
class Evaluator {
public:
    std::vector<Accumulator::AccumulatorTable> history{};
    std::unique_ptr<Accumulator::RefreshTable> refreshTable{};
    uint32_t historyIdx = 0;

    // Constructors for evaluator to allocate memory
    Evaluator();
    Evaluator(const Evaluator& e);
    Evaluator& operator=(const Evaluator& e);

    // Functions to reset the accumulator and history
    void reset(Position* pos);
    void reset_history();

    // Function to update the accumulator with a move made
    template<bool undo>
    void update_history(Position* pos, Move m, Piece pc, Piece cap);

    // Function for applying a lazy update of a given move
    void apply_lazy_updates(Position* pos, Color side, Move m, Piece pc, Piece cap);

    // Functions for evaluation
    Value predict(Position* pos);
    Value propagate(Color side);
};

}

#endif
