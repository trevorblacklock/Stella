#ifndef PERFT_H_INCLUDED
#define PERFT_H_INCLUDED

#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <deque>

#include "movegen.hpp"
#include "bitboard.hpp"
#include "position.hpp"
#include "misc.hpp"

namespace Stella {

struct PerftStats {
    Move m;
    uint64_t nodes;

    PerftStats(Move move, uint64_t val) {
        m = move;
        nodes = val;
    }

};

class Perft {
private:
    // Store number of threads
    int threadCount = 1;
    // Store number of running threads
    std::atomic_bool m_stop;
    std::atomic_int m_num_workers;
    std::mutex m_perft_mutex;
    std::mutex m_result_mutex;
    std::condition_variable m_perft_one;
    // Store each thread and it's relevant data
    std::vector<std::thread> threads;
    std::deque<PerftStats> perftResults;
    std::deque<Move> rootMoves;
    // Worker to execute perft when available
    void worker(Position* pos, Depth depth);
    // Executes perft search
    uint64_t perft(Position* pos, Depth depth);

public:
    // Main perft call
    void main_thread(Position *pos, Depth depth, int concurrency);
};

}

#endif
