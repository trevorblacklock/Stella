#include "search.hpp"
#include "position.hpp"
#include "bitboard.hpp"
#include "types.hpp"
#include "movegen.hpp"

#include <thread>
#include <cmath>

namespace Stella {

// Define constructor for setting thread using default constructor.
SearchData::SearchData(int id) : threadId(id) {}
// Default constructor
SearchData::SearchData() {}

template<Bound bound>
void Search::print_info_string() {
    assert(threadCount && threadData.size());

    // Get elapsed time
    int elapsed = tm->elapsed();

    // Retrieve relevent information to print from all threads
    uint64_t nodes = total_nodes();
    Depth seldepth = max_seldepth();

    // Rest of information is retrieved from main thread
    SearchData mainThread = threadData[0];
    Depth depth = mainThread.rootDepth;
    Value score = mainThread.score;
    PvLine pv = mainThread.pvTable[0];

    // Calculate the nodes per second
    uint64_t nps = (nodes * 1000) / (elapsed + 1);

    // Print the information
    std::cout << "info" << " depth " << depth << " seldepth " << seldepth;

    // Convert the score to Uci format
    if (std::abs(score) >= VALUE_MATE_IN_MAX_PLY)
        std::cout << " score mate" << (VALUE_MATE - std::abs(score) + 1) / 2 
                                    * (score > 0 ? 1 : -1);

    else std::cout << " score cp " << score;

    // Print a bound if given one
    if (bound)
        std::cout << (bound == BOUND_LOWER ? " lowerbound" : " upperbound") ;

    // Print search information
    std::cout << " nodes " << nodes << " nps " << nps << " time " << elapsed << " pv";

    // Print the pv line if one exists, otherwise print the best move
    if (pv.size)
        for (int i = 0; i < pv.size; i++) std::cout << " " << from_move(pv.moves[i], false);
    else
        std::cout << " " << from_move(mainThread.bestMove, false);

    // End with a newline
    std::cout << std::endl;
}

// Utility function to retrieve total nodes
uint64_t Search::total_nodes() const {
    uint64_t result = 0;
    for (auto& data : threadData)
        result += data.nodes;
    return result;
}

// Utility function to retrieve max seldepth
Depth Search::max_seldepth() const {
    Depth result = 0;
    for (auto& data : threadData)
        result = std::max(data.selDepth, result);
    return result;
}

// Set the number of threads in the search
void Search::set_threads(int num) {
    // Get the max number of threads
    int maxThreads = std::max(static_cast<uint32_t>(1), 
                     std::thread::hardware_concurrency());

    // Set the maximum from the argument given
    threadCount = std::clamp(num, 1, maxThreads);

    // Clear any old thread data
    threadData.clear();

    // Create the new threads
    for (int i = 0; i < threadCount; ++i)
        threadData.emplace_back();
}

// Main search function called from Uci.
// Sets up threads and calls them to run alpha beta function.
// Returns the best move found from the search, starts and stops all threads.
Move Search::search(Position *pos, TimeManager *manager, int id) {
    assert(pos);
    assert(manager);

    // Setup the search depth if given, otherwise set it to a maximum
    Depth depth = MAX_PLY - 1;

    if (manager->depthLimit.enabled)
        depth = std::min(depth, static_cast<Depth>(manager->depthLimit.max));

    // If this function is called from the main thread, then initialize
    // all other threads and setup any needed parameters
    if (!id) {
        // Store all the root moves
        Generator gen(pos);
        Move m;

        // Loop through the moves
        while ((m = gen.next_best<LEGAL>()) != Move::none())
            threadData[0].rootMoves.emplace_back(RootMove(m));

        // Setup the time manager
        tm = manager;
        assert(tm);
        
        // Reset each thread
        for (int i = 0; i < threadCount; ++i) {
            threadData[i] = threadData[0];
            threadData[i].threadId = i;
            threadData[i].nodes = 0;
            threadData[i].selDepth = 0;
            threadData[i].ply = 0;
            threadData[i].score = -VALUE_INFINITE;
            threadData[i].bestMove = Move::none();
        }

        // Call this function with each thread now,
        // this will skip this initialization
        for (int i = 1; i < threadCount; ++i) {
            threads.emplace_back(&Search::search, this, pos, manager, i);
        }
    }

    Value score = -VALUE_INFINITE;

    // Create new position for each thread so there is no memory overlap
    Position threadPos{*pos};
    SearchData *sd = &threadData[id];

    // Start iterative deepening loop
    Value average = -VALUE_INFINITE;

    Value alpha = -VALUE_INFINITE;
    Value beta = VALUE_INFINITE;
}

}