#include "search.hpp"
#include "position.hpp"
#include "types.hpp"
#include "movegen.hpp"

#include <thread>
#include <cmath>
#include <iostream>

namespace Stella {

void update_continuation_histories(Position* pos, History* hist, 
                                   int ply, Piece pc, Square to, int bonus);
void update_quiet_stats(Position* pos, History* hist, int ply, Move m, int bonus);
void update_history(Position* pos, History* hist, Generator* gen, 
                    int ply, Move best, int depth);

// Define constructor for setting thread using default constructor.
SearchData::SearchData(int id) : threadId(id) {}
// Default constructor
SearchData::SearchData() {}

// Clear SearchData
template<bool full>
void SearchData::clear() {
    if (full) hist.clear();
    pvTable.reset();
    ply = 0;
    rootDepth = 0;
    nodes = 0;
    selDepth = 0;
    nmpMinPly = 0;
    stop = false;
    score = -VALUE_INFINITE;
    bestMove = Move::none();
    extMove = Move::none();
}

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
        threadData.emplace_back(SearchData(i));
    }
}

// Clear the thread data, this is usually called when starting a new game
void Search::clear_thread_data() {
    // Loop through each thread
    for (auto& thread : threadData) {
        thread.clear<true>();
    }
}

void Search::init_lmr() {
    for (Depth depth = 0; depth < MAX_PLY; ++depth) {
        for (int ply = 0; ply < MAX_MOVES; ++ply) {
            if (depth == 0 || ply == 0) lmr[depth][ply] = 0;
            else lmr[depth][ply] = 1.25 + std::log(depth) * std::log(ply) * 1 / 3;
        }
    }
}

Depth Search::reductions(Depth depth, int legalMoves, Value delta, Value rootDelta) {
    return lmr[depth][legalMoves] + 1.5 - delta / rootDelta;
}

int stat_bonus(int depth) {
    return std::min(300 * depth - 250, 1500);
}

int stat_malus(int depth) {
    return std::min(350 * depth - 200, 1700);
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

        // Clear old root moves
        rootMoves.clear();

        // Loop through the moves
        while ((m = gen.next_best<LEGAL>()) != Move::none())
            rootMoves.emplace_back(RootMove(m));

        // Setup the time manager
        tm = manager;
        assert(tm);

        // Reset each thread
        for (auto& thread : threadData) {
            thread.clear<false>();
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

    Value average = -VALUE_INFINITE;

    // Main iterative deepening loop
    for (Depth depth = 1; depth <= maxDepth; ++depth) {
        // Reset the pv table
        sd->pvTable.reset();
        // Reset seldepth for this loop
        sd->selDepth = 0;
        // Set failedhigh counter to zero each iteration
        int failedHigh = 0;

        // Use aspiration windows to speed up searches based on recieved scores.
        // We can use the (weighted) average score to determine values for alpha & beta
        Value delta = 20 + average * average / 10000;
        Value alpha = std::clamp(average - delta, -VALUE_INFINITE, +VALUE_INFINITE);
        Value beta = std::clamp(average + delta, -VALUE_INFINITE, +VALUE_INFINITE);

        // Iterative deepening until search times out or score falls within aspiration window.
        while (tm->can_continue()) {
            // Set a new depth for the search based on the number of previous
            // searches that failed high.
            Depth newDepth = std::max(1, depth - failedHigh);

            sd->rootDelta = beta - alpha;
            sd->rootDepth = newDepth;

            // Find the bestmove and set the iterative average
            if (!sd->bestMove.is_none() && score != VALUE_INFINITE) {
                RootMove& rm = *std::find(rootMoves.begin(), rootMoves.end(), sd->bestMove);
                average = rm.averageScore;
            }

            // Run the search
            score = alphabeta<PV>(&newPos, sd, alpha, beta, newDepth);

            // Set previous scores
            for (RootMove& rm : rootMoves) {
                rm.previousScore = rm.currentScore;
            }

            // Set this score in the search data
            sd->score = score;

            // Check for a stop
            if (tm->forceStop) break;

            // Check if we must research with a different window
            if (score <= alpha) {
                failedHigh = 0;
                beta = (alpha + beta) / 2;
                alpha = std::max(score - delta, -VALUE_INFINITE);
                if (mainThread && infoStrings && tm->elapsed() >= 3000) 
                    print_info_string<BOUND_UPPER>();
            }

            else if (score >= beta) {
                failedHigh++;
                beta = std::min(score + delta, +VALUE_INFINITE);
                if (mainThread && infoStrings && tm->elapsed() >= 3000) 
                    print_info_string<BOUND_LOWER>();
            }

            else break;

            delta += delta / 3;
        }

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

    // Set the pv length to zero
    sd->pvTable[sd->ply].reset();

    // Check if depth is zero, if true then return an evaluation
    if (depth <= 0 || sd->ply >= MAX_PLY || depth >= MAX_PLY) 
        return qsearch<nodeType>(pos, sd, alpha, beta);

    assert(depth >= 0 && depth <= MAX_PLY);

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
    bool  improving = false;
    Key   key       = pos->key();
    Value bestScore = -VALUE_INFINITE;
    Value score     = -VALUE_INFINITE;
    Value standpat  = VALUE_NONE;
    Value eval      = VALUE_NONE;
    Move  bestMove  = Move::none();
    Move  ttMove    = Move::none();
    History* hist   = &sd->hist;
    Square prevSq   = pos->previous().move.is_ok() ? pos->previous().move.to() : SQ_NONE;

    // Check if a move draws from an upcoming repetition
    if (sd->ply && alpha < VALUE_DRAW && pos->has_game_cycled(sd->ply)) {
        alpha = 8 - (sd->nodes & 0xF);
        if (alpha >= beta) return alpha;
    }

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

    // Reset the previous killers
    hist->clear_killers_grandchildren(us, sd->ply);

    // Check for a transposition table entry
    TTentry* entry = table.probe(key, found);
    Value ttScore = found ? value_from_tt(entry->score(), sd->ply, pos->fifty_rule()) : VALUE_NONE;

    // Set the tt move
    ttMove = found ? entry->move() : Move::none();

    // Check if tt can be used for an early cutoff
    if (found
        && !pvNode
        && sd->extMove.is_none()
        && entry->depth() > depth - (entry->score() <= beta)
        && ttScore != VALUE_NONE
        && (entry->node() & (ttScore >= beta ? BOUND_LOWER : BOUND_UPPER))) {

        // If the ttMove is quiet, update move sorting heuristics
        if (!ttMove.is_none()) {
            if (!pos->is_capture(ttMove) && !pos->is_promotion(ttMove)) {
                if (ttScore >= beta)
                    update_quiet_stats(pos, hist, sd->ply, ttMove, stat_bonus(depth));
                else
                    update_continuation_histories(pos, hist, sd->ply, pos->piece_moved(ttMove), 
                                                  ttMove.to(), -stat_malus(depth));
            }
        }

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
        if (standpat == VALUE_NONE || is_extremity(standpat))
            standpat = eval = pos->evaluate();
        // If the transposition table has a score we can use that for standpat
        if (ttScore != VALUE_NONE
            && (entry->node() & (ttScore > eval ? BOUND_LOWER : BOUND_UPPER)))
            // Set the eval only, not standpat
            eval = ttScore;
    }
    // If no entry is found just evaluate normally
    else {
        standpat = eval = pos->evaluate();
    }

    // Set the historic eval with the standpat, rather than the
    // potentially adjusted eval from the hashtable.
    if (!inCheck) {
        hist->set_eval(us, sd->ply, standpat);
        improving = hist->is_improving(us, sd->ply, standpat);
    }

    // Razoring
    if (!inCheck
        && !pvNode
        && eval < alpha - 500 - 300 * depth * depth)
        return qsearch<nodeType>(pos, sd, alpha, beta);
    
    // Futility pruning
    if (!inCheck
        && !pvNode
        && depth < 10
        && eval - depth * 100 >= beta
        && !is_win(eval)
        && !is_loss(beta)
        && sd->extMove.is_none()
        && ttMove.is_none())
        return beta + (eval - beta) / 3;

    // Null move pruning
    if (!inCheck
        && !pvNode
        && sd->extMove.is_none()
        && !pos->gamestate()->move.is_none()
        && eval >= beta
        && eval >= standpat
        && standpat >= beta - 15 * depth - 200 * improving
        && pos->non_pawn_material(us)
        && !is_loss(beta)
        && !is_win(eval)
        && sd->ply >= sd->nmpMinPly) {

        // Setup depth reductions
        Depth nmpReduction = std::min((eval - beta) / 200, 6) + depth / 3 + 5;
        Depth nmpDepth = std::max(0, depth - nmpReduction);

        // Make the null move
        pos->do_null();
        Value val = -alphabeta<NON_PV>(pos, sd, -beta, 1 - beta, nmpDepth);
        pos->undo_null();

        // Do not return unproven winning scores
        if (val >= beta && !is_win(val)) {
            if (sd->nmpMinPly || depth < 14) return val;

            assert(!sd->nmpMinPly);
            
	          // Perform a verification serach at higher depth
            sd->nmpMinPly = sd->ply + 3 * (depth - nmpReduction) / 4;
            Value v = alphabeta<NON_PV>(pos, sd, beta - 1, beta, nmpDepth);
            sd->nmpMinPly = 0;

            // If value still fails high we can safely return the original
            if (v >= beta) return val;
        }
    }
    
    // Internal iterative deepening
    if (!inCheck && depth >= 4 && ttMove.is_none() && pvNode)
        depth -= 2;

    // Create move generator
    Generator gen(pos, hist, PV_SEARCH, ttMove, sd->ply);
    Move m;

    // Count the number of moves
    int legalMoves = 0;
    int moveCnt = 0;
    // Number of quiet moves we consider prior to skipping
    int quietPruning = (3 + depth * depth) / (2 - improving);

    // Loop through all pseudo-legal moves until none are left.
    // Legality is checked before playing a move rather than during
    // move generation, since often times due to hash moves and move
    // ordering aswell as the nature of the alpha-beta algorithm, a large
    // portion of generated moves will never have to be checked for legality.
    while ((m = gen.next()) != Move::none()) {

        // If this is an extension move we should skip it
        if (sd->extMove == m) continue;

        // Check the move here for legality
        if (!pos->is_legal(m)) continue;

        // Get some information about the move
        Square from        = m.from();
        Square to          = m.to();
        Piece  pc          = pos->piece_moved(m);
        bool   isCapture   = pos->is_capture(m);
        bool   isPromotion = pos->is_promotion(m);
        bool   givesCheck  = pos->gives_check(m);
        bool   quiet       = !isCapture && !isPromotion && !givesCheck;

        Value reduction = reductions(depth, legalMoves, beta - alpha, sd->rootDelta);
        Depth extension = 0;

        moveCnt++;

        // Move pruning
        if (sd->ply
            && pos->non_pawn_material(us)
            && legalMoves != 0
            && !is_loss(bestScore)) {

            // Crude depth estimation
            Depth moveDepth = std::max(1, depth - 1 - reduction);
            
            // If the number of searched moves is high enough, we can skip
            // the quiets, since move ordering dictates they will likely not be good
            if (moveCnt >= quietPruning) gen.skip_quiets();

            if (isCapture || givesCheck) {
                // Get the capture history
                Piece capPiece = pos->piece_on(to);
                Value history = hist->get_capture(pc, to, piece_type(capPiece));

                // Futility pruning for captures
                if (!givesCheck && !inCheck && moveDepth < 7) {
                    Value futility = standpat + 250 + 250 * moveDepth 
                                + piece_value(capPiece).mid + 100 * history / 1000;
                    // If we cannot improve alpha with a best
                    // case capture, we can skip this move
                    if (futility <= alpha) continue;
                }
            }
        }

        // Singular move extensions
        if (sd->ply < sd->rootDepth * 2
            && depth >= 8
            && sd->extMove.is_none()
            && m == ttMove
            && sd->ply > 0
            && entry->depth() >= depth - 3
            && ttScore != VALUE_NONE
            && !is_extremity(ttScore)
            && (entry->node() & BOUND_LOWER)) {
            // Compute values for singular beta and singular depth
            Value singularBeta = ttScore - depth * 2;
            Depth singularDepth = (depth - 1) / 2;

            assert(singularBeta > -VALUE_INFINITE && singularBeta < VALUE_INFINITE);

            // Set the extension move prior to search
            sd->extMove = m;
            score = alphabeta<NON_PV>(pos, sd, singularBeta - 1, singularBeta, singularDepth);
            sd->extMove = Move::none();
            
            // Set an extension if dont have a beta cutoff
            if (score < singularBeta)
                extension = 1 + !pvNode;
            
            // Given the bound of the ttentry, we can assume the ttMove will fail
            // high. If a reduced search excluding the ttMove still failes
            // high, we can assume this node is non-singular and prune the entire subtree.
            else if (score >= beta && !is_extremity(score))
                return score;

            // If other moves failed high on a reduced search without the ttMove,
            // yet the branch is not pruned since it does not exceed beta
            // we can reduce the ttMove in favor of other moves.
            else if (ttScore >= beta)
                extension = -2 - !pvNode;
        }

        // Set the next depth to search
        Depth newDepth = depth - 1 + extension;

        Value history;

        if (isCapture)
            history = 10 * piece_value(pos->piece_on(to)).mid
                    + hist->get_capture(pc, to, piece_type(pos->piece_on(to)))
                    - 5000;
        else if (inCheck)
            history = hist->get_butterfly(us, m)
                    + hist->get_continuation(pc, to, sd->ply - 1)
                    - 3000;
        else
            history = 2 * hist->get_butterfly(us, m)
                    + hist->get_continuation(pc, to, sd->ply - 1)
                    + hist->get_continuation(pc, to, sd->ply - 2)
                    - 3000;

        reduction += std::min(2, std::abs(eval - alpha) / 400);
        reduction -= hist->is_killer(us, m, sd->ply);
        reduction -= 2 * pvNode;
        reduction -= improving;
        reduction -= m == ttMove;
        reduction -= history / 10000;

        Depth reducedDepth = std::clamp(newDepth - reduction, 1, newDepth + 1);

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
        
        // Search with Late Move Redcutions
        // The conditions to enter LMR are simple, and the idea
        // is to search moves we can preemptively consider "bad"
        // at lower depths while not expecting much. If the node
        // raises the value of alpha, it induces a re-search since
        // the move was better than originally thought, and
        // move histories can then be updated accordingly
        // based on a more thourough search.
        if (depth >= 2 && moveCnt > 1) {   
            // Do the serach with reductions
            score = -alphabeta<NON_PV>(pos, sd, -alpha - 1, -alpha, reducedDepth);

            // Perform another search if the last score fails high with an actual
            // reduction
            if (score > alpha && reducedDepth < newDepth) {
                score = -alphabeta<NON_PV>(pos, sd, -alpha - 1, -alpha, newDepth);

                // Find a bonus to update continuation histories
                int bonus = score <= alpha ? -stat_malus(newDepth)
                          : score >= beta ? stat_bonus(newDepth) : 0;

                if (bonus)
                    update_continuation_histories(pos, hist, sd->ply, pc, to, bonus);
            }
        }

        // When the conditions for LMR are not met, we can enter a 
        // regular full-depth search
        else if (!pvNode || moveCnt > 1) {
            // Increase the reductions if ttMove is missing
            reduction += ttMove.is_none();
            Depth adjusted = newDepth - (reduction > 3) - (reduction > 5 && newDepth > 2);
            score = -alphabeta<NON_PV>(pos, sd, -alpha - 1, -alpha, adjusted);
        }

        // Lastly, for PV nodes we do a full search (sometimes a re-search)
        // called a Principle Variation Search (PVS) which is usually
        // the most computationally dependent, so we make sure the
        // score from previous searches atleast raises alpha to not
        // needlessly waste a search on an already known "bad" move.
        if (pvNode && (moveCnt == 1 || score > alpha)) {
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

        // If the search stopped we cannot trust the return value
        if (!tm->can_continue() || tm->forceStop) return beta;

        // Update the rootmoves average score for aspiration windows
        if (root) {
            // Find the root move
            RootMove& rm = *std::find(rootMoves.begin(), rootMoves.end(), m);
            rm.averageScore = (rm.averageScore != -VALUE_INFINITE) 
                            ? (2 * score + rm.averageScore) / 3 : score;
            rm.currentScore = score;
        }

        // If new best score found, update the score and best move
        if (score > bestScore) {
            // Update the scores and moves
            bestScore = score;
            bestMove = m;

            // If at a low depth and can continue set the best move
            if (root)
                sd->bestMove = m;

            // Update the pv at pv nodes
            if (pvNode && mainThread)
                sd->pvTable.update(m, sd->ply);

            // Check for a beta cutoff
            if (score >= beta) {
                // Store the result in the transposition table
                if (sd->extMove.is_none())
                    table.save<nodeType>(key, depth, value_to_tt(score, sd->ply), standpat, m, BOUND_LOWER);
                // Update the histories
                update_history(pos, hist, &gen, sd->ply, bestMove, depth);
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
    if (legalMoves == 0) {
        return !sd->extMove.is_none() ? alpha : inCheck ? mated_in(sd->ply) : VALUE_DRAW;
    }
    // Ensure the score falls within bounds
    assert(bestScore > -VALUE_INFINITE && bestScore < VALUE_INFINITE);

    // Store the score into the transposition table
    if (sd->extMove.is_none())
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
    Depth ttDepth   = 0;
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
        bestScore = standpat = -VALUE_INFINITE;
    }
    else {
        // For found transposition entries, try to find the standpat from there.
        if (found) {
            standpat = bestScore = entry->eval();
            // For values of none or extremeties, run an evaluate
            if (standpat == VALUE_NONE || is_extremity(standpat))
                bestScore = standpat = pos->evaluate();
            // If the transposition table has a score we can use that for standpat
            if (ttScore != VALUE_NONE
                && !is_extremity(ttScore)
                && (entry->node() & (ttScore > bestScore ? BOUND_LOWER : BOUND_UPPER)))
                // Set the eval only, not standpat
                bestScore = ttScore;
        }
        // If no entry is found just evaluate normally
        else {
            standpat = bestScore = pos->evaluate();
        }

        // Check for an early cutoff
        if (bestScore >= beta) {
            // If not already in the hashtable, we can add it now
            if (!found)
                table.save<NON_PV>(key, 0, value_to_tt(bestScore, sd->ply), 
                                   standpat, Move::none(), BOUND_NONE);
            // Return the score now
            return bestScore;
        }

        // Check for a fail high
        if (bestScore > alpha) {
            alpha = bestScore;
        }
    }

    // If at max ply, we just return the bestscore
    if (sd->ply >= MAX_PLY) return bestScore;

    Square prevSq = pos->previous().move.is_ok() ? pos->previous().move.to() : SQ_NONE;

    GenerationMode mode = inCheck ? QSEARCH_CHECK : QSEARCH;

    // Create a move generator for the position
    Generator gen(pos, hist, mode, Move::none(), sd->ply);
    Move m;
    int moveCnt = 0;

    // Loop through all the moves
    while ((m = gen.next()) != Move::none()) {

        // Get some information about the move
        Square from        = m.from();
        Square to          = m.to();
        Piece  pc          = pos->piece_moved(m);
        bool   isCapture   = pos->is_capture(m);
        bool   isPromotion = pos->is_promotion(m);
        bool   givesCheck  = pos->gives_check(m);
        bool   validSee    = !inCheck && (isCapture || isPromotion);

        // Check the legality of the move
        if (!pos->is_legal(m)) continue;

        // Pruning
        if (!is_loss(bestScore)) {
            // Futility pruning and movecount pruning
            if (!givesCheck && to != prevSq && !is_loss(standpat) && m.type() != PROMOTION) {
                // For an excessive number of moves (yes three) we can begin pruning
                if (moveCnt > 2) 
                    continue;

                // If standpat and the piece value of capture is much lower than alpha,
                // we can prune this move.
                if (alpha >= standpat + piece_value(pos->piece_on(to)).mid + 400) 
                    continue;

                // If static exchange is low we can prune
                Value see = isCapture ? gen.see_value() : pos->see(m);
                if (see <= alpha - standpat - 400) 
                    continue;
            }

            // Continuation history based pruning
            if (!isCapture && hist->get_continuation(pc, to, sd->ply - 1) <= 2000)
                continue;

            // Static exchange evaluation based pruning
            if (validSee) {
                Value see = validSee ? gen.see_value() : 0;
                // Values under zero represent bad captures
                if (see < -50) continue;
            }
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
            
            if (score > alpha) {
                bestMove = m;
                
                // Check for beta cutoff
                if (score < beta)
                    alpha = score;
                else {
                    ttDepth = givesCheck;
                    break;
                }
            }        
        }
    }

    // If there are no moves, just return a base evaluation
    if (inCheck && bestScore == -VALUE_INFINITE) {
        assert(!moveCnt);
        return mated_in(sd->ply);
    }

    assert(bestScore > -VALUE_INFINITE && bestScore < VALUE_INFINITE);

    // If there is moves, store the best value in the transposition table.
    // The depth is determined by if there is a beta cutoff and in check.
    if (!bestMove.is_none())
        table.save<nodeType>(key, ttDepth, value_to_tt(bestScore, sd->ply), standpat, bestMove,
                             bestScore >= beta ? BOUND_LOWER : BOUND_UPPER);

    // Return the best score
    return bestScore;
}

void update_continuation_histories(Position* pos, History* hist, 
                                   int ply, Piece pc, Square to, int bonus) {
    for (int i : {1, 2, 3, 4, 6}) {
        // Only update first two if in check
        if (pos->checks() && i > 2) 
            break;
        if (pos->previous(i).move.is_ok()) {
            hist->update_continuation(pc, to, ply - i, bonus);
        }
    }
}

void update_quiet_stats(Position* pos, History* hist, int ply, Move m, int bonus) {
    Color us = pos->side();

    // Update killer moves
    if (!pos->is_promotion(m))
        hist->set_killer(us, m, ply);

    // Update main and continuation history
    hist->update_butterfly(us, m, bonus);
    update_continuation_histories(pos, hist, ply, pos->piece_moved(m), m.to(), bonus);
}

void update_history(Position* pos, History* hist, Generator* gen, 
                    int ply, Move best, int depth) {
    Piece pc;
    Square to;
    PieceType cap;
    
    MoveList searched = gen->get_searched_list();
    
    int bonus = stat_bonus(depth);
    int malus = stat_malus(depth);

    if (pos->is_capture(best)) {
        pc = pos->piece_moved(best);
        to = best.to();
        cap = piece_type(pos->piece_on(to));
        hist->update_capture(pc, to, cap, bonus);
    }

    else {
        update_quiet_stats(pos, hist, ply, best, bonus);

        for (int i = 0; i < searched.size; ++i) {
            Move m = searched.moves[i];
            if (!pos->is_capture(m) && m != best) {
                update_quiet_stats(pos, hist, ply, m, -malus);
            }
        }
    }

    for (int i = 0; i < searched.size; ++i) {
        Move m = searched.moves[i];
        if (pos->is_capture(m) && m != best) {
            pc = pos->piece_moved(m);
            to = m.to();
            cap = piece_type(pos->piece_on(to));
            hist->update_capture(pc, to, cap, -malus);
        }
    }
}

}
