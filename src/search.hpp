#ifndef SEARCH_H_INCLUDED
#define SEARCH_H_INCLUDED

#include "position.hpp"
#include "movegen.hpp"
#include "misc.hpp"
#include "timing.hpp"
#include "tt.hpp"
#include "history.hpp"
#include "pv.hpp"
#include "history.hpp"

#include <thread>

namespace Stella {

// Structure for storing a "Root Move" which is a move that exists in the root position
struct RootMove {
    // Store the move
    Move m;

    // Create an operator to check if one Root Move is equal to another
    bool operator==(const Move& move) { return move == m; }

    // Store average, current and previous score found through search
    Value averageScore =  -VALUE_INFINITE;
    Value previousScore = -VALUE_INFINITE;
    Value currentScore =  -VALUE_INFINITE;

    // Constructor for a Root Move
    RootMove(Move move) { m = move; }
};

// Structure for storing information to be continuously updated and used through recursive search.
// Inherits move histories which are updated ply by ply in a new instance.
struct SearchData {
    int threadId;
    int ply;
    int rootDepth;
    bool stop;
    Pv pvTable;
    History hist;

    uint64_t nodes;
    Depth selDepth;
    Value score;
    Move bestMove;

    SearchData();
    explicit SearchData(int id);

    // Function to clear searchdata
    template<bool full>
    void clear();
};

// Class the manage the search, allocation of threads and time management
class Search {
private:
    // Store number of threads
    int threadCount = 1;
    // Store each thread and it's relevent data
    std::vector<std::thread> threads;
    std::vector<SearchData> threadData;

    // Store the rootmoves of the position
    std::vector<RootMove> rootMoves;

    // Flag for enabling/disabling info strings
    bool infoStrings = true;
    // Flag for chess960
    bool chess960 = false;

    // Time manager
    TimeManager* tm;

public:
    // Set info string
    void set_info_string(bool val) { infoStrings = val; }
    // Function for printing info string to the shell
    template<Bound bound>
    void print_info_string();
    // Set the number of threads
    void set_threads(int num);
    // Stop threads
    void stop();
    // Clear all the thread data
    void clear_thread_data();

    // Main search function to launch threads
    Move search(Position* pos, TimeManager* manager, int id = 0);

    // Alpha beta pruning function, takes into account many heuristics to return an evaluation
    template<NodeType nodeType>
    Value alphabeta(Position* pos, SearchData* sd, Value alpha, Value beta, Depth depth);

    // Qsearch function for evaluating a position taking into account the available captures
    template<NodeType nodeType>
    Value qsearch(Position* pos, SearchData* sd, Value alpha, Value beta);

    // Utilities for extracting data from all threads
    uint64_t total_nodes() const;
    Depth max_seldepth() const;
};

}

#endif
