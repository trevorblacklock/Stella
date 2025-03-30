#include "search.hpp"
#include "position.hpp"
#include "types.hpp"
#include "movegen.hpp"

#include <thread>
#include <cmath>
#include <iostream>

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
    SearchData* mainThread = &threadData[0];
    Depth depth = mainThread->rootDepth;
    Value score = mainThread->score;
    PvLine pv = mainThread->pvTable[0];

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
    std::cout << " nodes " << nodes << " nps " << nps << " time " << elapsed << " hashfull " << table.hashfull() << " pv";

    // Print the pv line if one exists, otherwise print the best move
    if (pv.size)
        for (int i = 0; i < pv.size; i++) std::cout << " " << from_move(pv.moves[i], chess960);
    else
        std::cout << " " << from_move(mainThread->bestMove, chess960);

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
Move Search::search(Position* pos, TimeManager* manager, int id) {
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
        // Set a new search for the transposition table
        table.new_search();
        // Set chess960 flag
        chess960 = pos->is_chess960();

        // Store all the root moves
        Generator gen(pos);
        Move m;

        // Loop through the moves
        while ((m = gen.next_best<LEGAL>()) != Move::none())
            rootMoves.emplace_back(RootMove(m));

        // Setup the time manager
        tm = manager;
        assert(tm);

        // Reset each thread
        for (int i = 0; i < threadCount; i++) {
            // Allocate a stack up to max ply for search data, using ply + 7 for continuation histories
            threadData[i].threadId = i;
            threadData[i].stop = false;
            threadData[i].nodes = 0;
            threadData[i].selDepth = 0;
            threadData[i].ply = 0;
            threadData[i].score = -VALUE_INFINITE;
            threadData[i].bestMove = Move::none();
        }

        // Call this function with each non main thread now,
        // this will skip this initialization
        for (int i = 1; i < threadCount; ++i) {
            threads.emplace_back(&Search::search, this, pos, manager, i);
        }
    }

    Value score = -VALUE_INFINITE;

    // Create new position for each thread so there is no memory overlap
    Position newPos = *pos;
    SearchData* sd = &threadData[id];

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
        score = alphabeta<PV>(&newPos, sd, alpha, beta, depth);

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

template<NodeType nodeType>
Value Search::alphabeta(Position* pos, SearchData* sd, 
                        Value alpha, Value beta, Depth depth) {
    // Get some definitions for the current search
    Color us = pos->side();
    bool root = sd->ply == 0;
    bool pvNode = nodeType == PV;
    bool mainThread = sd->threadId == 0;

    // Assert some things
    assert(sd->ply >= 0 && sd->ply <= MAX_PLY);
    assert(alpha >= -VALUE_INFINITE);
    assert(beta <= VALUE_INFINITE);
    assert(beta > alpha);
    assert(depth >= 0 && depth < MAX_PLY);

    // Set the pv length to zero
    sd->pvTable[sd->ply].reset();

    // Check if depth is zero, if true then return an evaluation
    if (depth <= 0) return qsearch<nodeType>(pos, sd, alpha, beta);

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

    // Increment nodes
    sd->nodes++;

    // Update the seldepth
    if (sd->ply + 1 > sd->selDepth) sd->selDepth = sd->ply + 1;

    // Get all search info needed
    bool  inCheck   = pos->checks();
    bool  found     = false;
    Key   key       = pos->key();
    Value bestScore = -VALUE_INFINITE;
    Value score     = -VALUE_INFINITE;
    Value standpat  = VALUE_NONE;
    Value eval      = VALUE_NONE;
    Move  bestMove  = Move::none();
    Move  ttMove    = Move::none();
    History* hist   = &sd->hist;

    // Check for a draw
    if (sd->ply && pos->is_draw())
        // Draw randomization based on node count
        return 8 - (sd->nodes & 0xF);

    // Check for max ply value and not in check, to return eval
    if (sd->ply >= MAX_PLY)
        return !inCheck ? pos->evaluate() : VALUE_DRAW;

    // Mate distance pruning
    if (sd->ply) {
        alpha = std::max(mated_in(sd->ply), alpha);
        beta = std::min(mate_in(sd->ply + 1), beta);
        if (alpha >= beta) return alpha;
    }

    // Check for a transposition table entry
    TTentry* entry = table.probe(key, found);
    Value ttScore = found ? value_from_tt(entry->score(), sd->ply, pos->fifty_rule()) : VALUE_NONE;

    // Set the tt move
    ttMove = found ? entry->move() : Move::none();

    // Check if tt can be used for an early cutoff
    if (found
        && !pvNode
        && entry->depth() > depth - (entry->score() <= beta)
        && ttScore != VALUE_NONE
        && (entry->node() & (ttScore >= beta ? BOUND_LOWER : BOUND_UPPER))) {

        // So long as fifty move rule is not large, can return the score
        if (pos->fifty_rule() < 90) return ttScore;
    }

    // Set standpat to a mate value for checks
    if (inCheck) {
        standpat = eval = -VALUE_MATE + sd->ply;
    }
    // For found transposition entries, try to find the standpat from there.
    else if (found) {
        standpat = eval = entry->eval();
        // For values of none, run an evaluate
        if (eval == VALUE_NONE)
            standpat = eval = pos->evaluate();
        // If the transposition table has a score we can use that for standpat
        if (ttScore != VALUE_NONE
            && (entry->node() & (ttScore >= beta ? BOUND_LOWER : BOUND_UPPER)))
            // Set the eval only, not standpat
            eval = ttScore;
    }
    // If no entry is found just evaluate normally
    else {
        standpat = eval = pos->evaluate();
    }

    // Create move generator
    Generator gen(pos, hist, PV_SEARCH, ttMove, sd->ply);
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
        if (root && mainThread && infoStrings && !tm->forceStop && tm->elapsed() > 3000) {
            std::cout << "info depth "
                      << sd->rootDepth
                      << " currmove "
                      << from_move(m, chess960)
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
            score = -alphabeta<PV>(pos, sd, -beta, -alpha, newDepth);
        else {
            score = -alphabeta<NON_PV>(pos, sd, -alpha - 1, -alpha, newDepth);
            // Do a full re-search if score is with alpha and beta bounds
            if (score > alpha && score < beta)
                score = -alphabeta<PV>(pos, sd, -beta, -alpha, newDepth);
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
            if (root && tm->can_continue())
                sd->bestMove = m;

            // Update the pv at pv nodes
            if (pvNode && mainThread && !tm->forceStop)
                sd->pvTable.update(m, sd->ply);

            // Check for a beta cutoff
            if (score >= beta) {
                // Store the result in the transposition table
                table.save<nodeType>(key, depth, value_to_tt(score, sd->ply), standpat, m, BOUND_LOWER);
                // Return the score
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

    // Store the score into the transposition table
    table.save<nodeType>(key, depth, value_to_tt(bestScore, sd->ply), standpat, bestMove,
                        (bestMove != Move::none() && pvNode) ? BOUND_EXACT : BOUND_UPPER);

    // Return the best score
    return bestScore;
}

template<NodeType nodeType>
Value Search::qsearch(Position* pos, SearchData* sd, Value alpha, Value beta) {
    // Ensure values are within bounds
    assert(sd->ply >= 0 && sd->ply <= MAX_PLY);
    assert(alpha >= -VALUE_INFINITE);
    assert(beta <= VALUE_INFINITE);
    assert(beta > alpha);

    bool pvNode = nodeType == PV;
    bool root = sd->ply == 0;
    bool mainThread = sd->threadId == 0;

    Color us = pos->side();

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

    // Increment the nodes
    sd->nodes++;

    // Update seldepth
    if (sd->ply + 1 > sd->selDepth) sd->selDepth = sd->ply + 1;

    // Get all search info needed
    bool  found     = false;
    bool  inCheck   = pos->checks();
    Key   key       = pos->key();
    Value bestScore = -VALUE_INFINITE;
    Value score     = -VALUE_INFINITE;
    Value standpat  = VALUE_NONE;
    Move  bestMove  = Move::none();
    Move  ttMove    = Move::none();
    History* hist   = &sd->hist;

    // Check for a draw
    if (sd->ply && pos->is_draw())
        // Draw randomization based on node count
        return 8 - (sd->nodes & 0xF);

    // Check for max ply value and not in check, to return eval
    if (sd->ply >= MAX_PLY)
        return !inCheck ? pos->evaluate() : VALUE_DRAW;

    // Check for a transposition table entry
    TTentry* entry = table.probe(key, found);
    Value ttScore = found ? value_from_tt(entry->score(), sd->ply, pos->fifty_rule()) : VALUE_NONE;

    // Check if tt can be used for an early cutoff
    if (found
        && !pvNode
        && entry->depth() >= inCheck
        && ttScore != VALUE_NONE
        && (entry->node() & (ttScore >= beta ? BOUND_LOWER : BOUND_UPPER))) {

        // Return the score
        return ttScore;
    }

    // Set standpat and bestscore to a mate value for checks
    if (inCheck) {
        bestScore = -VALUE_INFINITE;
    }
    else {
        // For found transposition entries, try to find the standpat from there.
        if (found) {
            standpat = bestScore = entry->eval();
            // For values of none, run an evaluate
            if (std::abs(bestScore) >= VALUE_WIN_MAX_PLY)
                standpat = bestScore = pos->evaluate();
            // If the transposition table has a score we can use that for standpat
            if (ttScore != VALUE_NONE
                && std::abs(ttScore) < VALUE_WIN_MAX_PLY
                && (entry->node() & (ttScore >= beta ? BOUND_LOWER : BOUND_UPPER)))
                // Set the eval only, not standpat
                bestScore = ttScore;
        }
        // If no entry is found just evaluate normally
        else {
            standpat = bestScore = pos->evaluate();
        }

        // Check for an early cutoff
        if (bestScore >= beta) {
            // Return the score now
            return bestScore;
        }

        // Check for a fail high
        if (bestScore > alpha) {
            alpha = bestScore;
        }
    }

    GenerationMode mode = inCheck ? QSEARCH_CHECK : QSEARCH;

    // Create a move generator for the position
    Generator gen(pos, hist, mode, Move::none(), sd->ply);
    Move m;
    int moveCnt = 0;

    // Loop through all the moves
    while ((m = gen.next()) != Move::none()) {
        // Check the legality of the move
        if (!pos->is_legal(m)) continue;

        // Check if the move is a capture or not
        bool isCapture = pos->is_capture(m);

        // get the current see value for this move
        Value see = gen.see_value();

        // Pruning for current position
        if (bestScore > VALUE_LOSS_MAX_PLY && pos->non_pawn_material(us)) {
            if (isCapture && see < 0) continue;
        }

        moveCnt++;

        // Make the move
        pos->do_move<true>(m);
        // Increment the ply
        sd->ply++;

        // Recursive call to qsearch
        score = -qsearch<nodeType>(pos, sd, -beta, -alpha);
        // Ensure score falls within bounds
        assert(score > -VALUE_INFINITE && score < VALUE_INFINITE);

        // Undo the move
        pos->undo_move(m);
        // Decrement the ply
        sd->ply--;

        // Check for a new bestscore
        if (score > bestScore) {
            // Update the bestscore
            bestScore = score;
            bestMove = m;

            if (!pvNode && mainThread && !tm->forceStop)
                sd->pvTable.update(m, sd->ply);

            // Check for beta cutoff
            if (score < beta)
                alpha = score;
            else
                break;
        }
    }

    // If there are no moves, just return a base evaluation
    if (inCheck && bestScore == -VALUE_INFINITE) {
        assert(!moveCnt);
        assert(!Generator(pos).count<LEGAL>());
        return mated_in(sd->ply);
    }

    assert(bestScore > -VALUE_INFINITE && bestScore < VALUE_INFINITE);

    // If there is moves, store the best value in the transposition table.
    // The depth is determined by if there is a beta cutoff and in check.
    if (moveCnt)
        table.save<nodeType>(key, 0, value_to_tt(bestScore, sd->ply), standpat, bestMove,
                            bestScore >= beta ? BOUND_LOWER : BOUND_UPPER);

    // Return the best score
    return bestScore;
}

}
