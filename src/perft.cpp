#include "perft.hpp"
#include "misc.hpp"

#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

namespace Stella {

void Perft::main_thread(Position* pos, Depth depth, int concurrency) {
    assert(pos);
    assert(depth);

    // Create a timer
    Timer timer;

    // Keep track of the total nodes to print at the end
    uint64_t totalNodes = 0;

    // Get the max number of threads
    int maxThreads = std::max(static_cast<uint32_t>(1),
                    std::thread::hardware_concurrency());

    // At low depths clamp the threads to 1, since multi threading
    // on such a fast scale can be rather unstable and can lead to
    // unknown behaviour
    maxThreads = depth > 3 ? maxThreads : 1;

    threadCount = std::clamp(concurrency, 1, maxThreads);

    // Set atomic variables
    m_num_workers.store(0);
    m_stop.store(false);

    // Get all the root moves
    Generator gen(pos);
    Move m;

    // Loop through the moves
    while ((m = gen.next_best<LEGAL>()) != Move::none())
        rootMoves.emplace_back(m);

    // Start timer before calling threads
    timer.start();

    // Dispatch threads to run on this function, will not enter
    // this scope since id will be non zero
    for (int i = 0; i < threadCount; ++i) {
        threads.emplace_back(&Perft::worker, this, pos, depth);
        m_num_workers.fetch_add(1);
    }

    // Loop through while not stopped and take results printing them to the console
    while (true) {
        // Set a lock to access the results, only grab when conditional is met
        std::unique_lock<std::mutex> lock(m_result_mutex);
        // First check if stop is triggered and results are empty
        if (m_stop.load() && perftResults.empty() && m_num_workers.load() == 0) break;

        m_perft_one.wait(lock, [this]() { return !perftResults.empty(); });

        // Get the first value in the vector and then pop it
        PerftStats result = perftResults.front();
        perftResults.pop_front();

        lock.unlock();

        totalNodes += result.nodes;

        // Print the info here
        std::cout << from_move(result.m, pos->is_chess960()) << ": " << result.nodes << std::endl;
    }

    // End timer once loop is complete
    timer.end();

    // Check threads to be complete to close them
    for (auto& thread : threads) thread.join();

    // Print final result
    std::cout << std::endl << "Search results: " << std::endl << "==============" << std::endl;
    std::cout << totalNodes << " Nodes" << std::endl;
    std::cout << timer.elapsed() << " ms" << std::endl;
}

void Perft::worker(Position* pos, Depth depth) {
  while (!m_stop.load()) {
    // Check the root moves to ensure there is work to do
    Move m;
    { 
        // Create a lock to access the deque
        std::unique_lock<std::mutex> lock(m_perft_mutex);
        // If the deque is empty, sub one worker and exit scope
        if (rootMoves.empty()) break;

        // Get next move
        m = rootMoves.front();
        rootMoves.pop_front();
    }

    // Using the move generate a position from the
    // existing one passed to the worker funciton
    Position newPos = *pos;
    newPos.do_move<false>(m);

    // Run the perft search adding an extra node to account
    // for the root move made just above.
    uint64_t nodes = Perft::perft(&newPos, depth - 1);

    {
      // Create a lock to access the vector
      std::unique_lock<std::mutex> lock(m_result_mutex);
      // Add the root move and node count
      perftResults.emplace_back(PerftStats(m, nodes));
    }

    m_perft_one.notify_one();
  }
  
  m_stop.store(true);
  m_num_workers.fetch_sub(1);
}

// A tool used to verify move generation. Given a depth will traverse every legal move
// up to that depth and return the number of nodes traversed.
uint64_t Perft::perft(Position* pos, Depth depth) {
    // If depth is 0 just return 0 nodes
    if (depth <= 0) return 1;
    // Set initial count to zero
    uint64_t cnt,nodes = 0;
    // Initialize move
    Move m;

    // Check if depth is 2, if so can implement a trick during search
    const bool leaf = (depth == 2);
    // Initialize a move generator for the position
    Generator gen(pos);

    // Loop through every legal move
    while ((m = gen.next_best<LEGAL>()) != Move::none()) {
        // Make the move
        pos->do_move<false>(m);
        // If the depth is two can count all root moves from the next position, otherwise recursively call perft
        cnt = leaf ? Generator(pos).count<LEGAL>() : perft(pos, depth - 1);
        // Count the nodes
        nodes += cnt;
        // Undo the move
        pos->undo_move(m);
    }
    // Return the total node count
    return nodes;
}

}
