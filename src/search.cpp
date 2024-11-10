#include "search.hpp"
#include "position.hpp"
#include "bitboard.hpp"
#include "evaluate.hpp"
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
        std::cout << " score mate " << (VALUE_MATE - std::abs(score) + 1) / 2
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
    for (int i = 0; i < threadCount; ++i) {
        threadData.emplace_back();
    }
}

// Main search function called from Uci.
// Sets up threads and calls them to run alpha beta function.
// Returns the best move found from the search, starts and stops all threads.
Move Search::search(Position *pos, TimeManager *manager, int id) {
    assert(pos);
    assert(manager);

    // Check if is main thread
    bool mainThread = id == 0;

    // Set the manager defaults
    manager->forceStop = false;

    // Setup the search depth if given, otherwise set it to a maximum
    Depth maxDepth = MAX_PLY - 1;

    if (manager->depthLimit.enabled)
        maxDepth = std::min(maxDepth, static_cast<Depth>(manager->depthLimit.max));

    // If this function is called from the main thread, then initialize
    // all other threads and setup any needed parameters
    if (mainThread) {
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
        for (int i = 0; i < threadCount; i++) {
            threadData[i].rootMoves = threadData[0].rootMoves;
            threadData[i].threadId = i;
            threadData[i].stop = false;
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
    Position newPos = *pos;
    SearchData *sd = &threadData[id];

    // Start iterative deepening loop
    Value average = -VALUE_INFINITE;

    Value alpha = -VALUE_INFINITE;
    Value beta = VALUE_INFINITE;

    // Main iterative deepening loop
    for (Depth depth = 1; depth <= maxDepth; ++depth) {
        // Reset the pv table
        sd->pvTable.reset();
        // Reset seldepth for this loop
        sd->selDepth = 0;
        // Set the root depth
        sd->rootDepth = depth;

        // Run the search
        score = alphabeta(&newPos, sd, alpha, beta, depth);

        // Set this score in the search data
        sd->score = score;

        // Check for a stop
        if (!tm->can_continue()) break;

        // If no stop can print out info strings for this depth
        if (mainThread && this->infoStrings)
            print_info_string<BOUND_NONE>();
    }

    // When the main thread is finished return the best move
    // and close all other threads
    if (mainThread) {
        // Stop the search if any threads are still going
        tm->stop();
        // Join all threads
        for (auto& thread : threads) thread.join();

        // get the bestmove
        Move best = sd->bestMove;
        // Clear all thread data
        threads.clear();

        // Return the best move
        return best;
    }

    return Move::none();
}

Value Search::alphabeta(Position* pos, SearchData* sd, Value alpha, Value beta, Depth depth) {
    // Get some definitions for the current search
    Color us = pos->side();
    bool pvNode = (beta - alpha) != 1;
    bool root = sd->ply == 0;
    bool mainThread = sd->threadId == 0;

    // Assert some things
    assert(sd->ply >= 0 && sd->ply < MAX_PLY);
    assert(alpha >= -VALUE_INFINITE);
    assert(beta <= VALUE_INFINITE);
    assert(beta > alpha);
    assert(depth >= 0 && depth < MAX_PLY);

    // Set the pv length to zero
    sd->pvTable[sd->ply].reset();

    // Check if depth is zero, if true then return an evaluation
    if (depth == 0) return Evaluate::evaluate(pos);

    // Check for a force stop
    if (tm->forceStop) {
        return beta;
    }

    // Check if time is out every 1024 nodes, if true then fail high
    if (sd->nodes % 1024 == 0
        && sd->threadId == 0
        && !tm->can_continue()) {
        // Stop the search and fail high
        tm->stop();
        return beta;
    }

    // Update the seldepth
    if (sd->ply > sd->selDepth) sd->selDepth = sd->ply;

    // Increment nodes
    sd->nodes++;

    // Get all search info needed
    bool  inCheck   = pos->checks();
    Key   key       = pos->key();
    Value bestScore = -VALUE_INFINITE;
    Value score     = -VALUE_INFINITE;
    Value standpat  = VALUE_NONE;
    Value eval      = VALUE_NONE;
    Move  bestMove  = Move::none();

    // Check for a draw
    if (sd->ply && pos->is_draw())
        // Draw randomization based on node count
        return 8 - (sd->nodes & 0xF);

    // Check for max ply value and not in check, to return eval
    if (sd->ply == MAX_PLY - 1 && !inCheck)
        return Evaluate::evaluate(pos);

    // Mate distance pruning
    if (sd->ply) {
        alpha = std::max(mated_in(sd->ply), alpha);
        beta = std::max(mate_in(sd->ply + 1), beta);
        if (alpha >= beta) return alpha;
    }

    // Create move generator
    Generator gen(pos, sd->ply);
    Move m;

    // Count the number of moves
    int legalMoves = 0;
    int moveCnt = 0;

    // Loop through all pseudo-legal moves until none are left.
    // Legality is checked before playing a move rather than during
    // move generation, since often times due to hash moves and move
    // ordering aswell as the nature of the alpha-beta algorithm, a large
    // portion of generated moves will never have to be checked for legality.
    while ((m = gen.next()) != Move::none()) {
        // Check the move here for legality
        if (!pos->is_legal(m)) continue;

        // Get some information about the move
        Square from        = m.from();
        Square to          = m.to();
        Piece  pc          = pos->piece_moved(m);
        bool   isCapture   = pos->is_capture(m);
        bool   isPromotion = pos->is_promotion(m);
        // bool   givesCheck  = pos->gives_check(m);
        // bool   quiet       = !isCapture && !isPromotion && !givesCheck;

        // Set the next depth to search
        Depth newDepth = depth - 1;

        // Increment the move counter
        moveCnt++;

        // Send information about the current move if enough time has passed
        if (root && mainThread && infoStrings && tm->elapsed() > 3000) {
            std::cout << "info depth "
                      << sd->rootDepth
                      << " currmove "
                      << from_move(m, pos->is_chess960())
                      << " currmovenumber "
                      << moveCnt
                      << std::endl;
        }

        // Make the move
        pos->do_move<true>(m);

        // Increment the ply counter
        sd->ply++;

        // Principle Variation Search (PVS)
        if (legalMoves == 0)
            score = -alphabeta(pos, sd, -beta, -alpha, newDepth);
        else {
            score = -alphabeta(pos, sd, -alpha - 1, -alpha, newDepth);
            // Do a full re-search if score is with alpha and beta bounds
            if (score > alpha && score < beta)
                score = -alphabeta(pos, sd, -beta, -alpha, newDepth);
        }

        // Ensure the score falls within bounds
        assert(score > -VALUE_INFINITE && score < VALUE_INFINITE);

        // Undo the move and add it to the list of searched moves
        pos->undo_move(m);
        gen.add_searched(m);

        // Decrement the ply counter
        sd->ply--;

        // Increment the legal moves
        legalMoves++;

        // If new best score found, update the score and best move
        if (score > bestScore) {
            // Update the scores and moves
            bestScore = score;
            bestMove = m;

            // If at a low depth and can continue set the best move
            if (!sd->ply && (tm->can_continue() || depth <= 2))
                sd->bestMove = m;

            // Update the pv at pv nodes
            if (pvNode && mainThread && !tm->forceStop)
                sd->pvTable.update(m, sd->ply);

            // Check for a beta cutoff
            if (score >= beta) {
                return score;
            }
            // Check for a fail high
            else if (score > alpha) {
                alpha = score;
            }
        }
    }

    // If there are no legal moves then its either stalemate of checkmate.
    // Can figure it out from knowing if we are in currently in check
    if (legalMoves == 0) bestScore = inCheck ? mated_in(sd->ply) : VALUE_DRAW;
    // Ensure the score falls within bounds
    assert(bestScore > -VALUE_INFINITE && bestScore < VALUE_INFINITE);

    // Return the best score
    return bestScore;
}

}
