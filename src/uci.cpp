#include "uci.hpp"
#include "perft.hpp"
#include "types.hpp"
#include "movegen.hpp"
#include "position.hpp"
#include "tt.hpp"
#include "misc.hpp"
#include "search.hpp"
#include "timing.hpp"
#include "nn/evaluate.hpp"

#include <iostream>
#include <thread>
#include <vector>

namespace Stella {

// Check if every element of a string is a number
inline bool is_number(std::string str) {
    // Loop through each character
    for (char& c : str) {
        if (!std::isdigit(c)) return false;
    }
    // If passed, can return true
    return true;
}

// Given a subset of the string, finds the string next over seperated by a space.
// For example, given "hello world" and the token "hello", will return "world".
// This is useful for parsing uci commands that are followed with a value.
// If key is not found, returns an empty string.
inline std::string find_val_from_key(std::string str, std::string key) {
    // Split the string with space as delimiter
    std::vector<std::string> strVector = split(str, ' ');

    // Store the size of the vector, ensuring to not over-index
    int numWords = strVector.size();

    // Loop through the words
    for (int i = 0; i < numWords; ++i) {
        // If key matches and a following word exists, return the next word.
        if (strVector[i] == key && i + 1 < numWords) {
            return strVector[i + 1];
        }
    }

    // Otherwise return an empty string
    return "";
}

// Given a subset of the string, finds the string next over and set the given value.
// Defaults to zero if value passed is not a number.
template <typename T>
inline T get_val_from_key(std::string str, std::string key) {
    // Retrieve the str value
    std::string val = find_val_from_key(str, key);
    // Return the casted result
    return (!val.empty() && is_number(val)) ? static_cast<T>(std::stoi(val))
                                                        : static_cast<T>(0);
}

template<uint64_t>
inline uint64_t get_val_from_key(std::string str, std::string key) {
    // Retrieve the str value
    std::string val = find_val_from_key(str, key);
    // Return the casted result
    return (!val.empty() && is_number(val)) ? std::stoull(val) : 0;
}

Uci::Uci() {
    // Set default threads
    s.set_threads(1);
    // Init the lmr array
    s.init_lmr();
    // Set default transposition table size
    table.resize(16);
}

Uci::~Uci() {
    quit();
}

const Move Uci::to_move(std::string move) {
    // Convert move to lowercase
    move = to_lower(move);

    // Generate a list of legal moves in the position
    Generator gen(&pos);

    // Store the current move
    Move m;

    // Loop through all legal moves
    while ((m = gen.next_best<LEGAL>()) != Move::none()) {
        if (move == from_move(m, pos.is_chess960())) return m;
    }

    // return nothing if move was not found
    return Move::none();
}

void Uci::uci() {
    // Print info about the engine
    std::cout << "id name Stella "
              << MAJOR_VERSION
              << '.'
              << MINOR_VERSION
              << " by T. Blacklock"
              << std::endl
              << "option name Hash type spin default 16 min 1 max "
              << table.max_size()
              << std::endl
              << "option name Threads type spin default 1 min 1 max "
              << std::thread::hardware_concurrency()
              << std::endl
              << "option name MoveOverhead type spin default 0 min 0 max 1000"
              << std::endl
              << "uciok"
              << std::endl;
}

void Uci::loop(int argc, char* argv[]) {

    // Send gui the engines information
    std::cout << "Stella "
              << MAJOR_VERSION
              << '.'
              << MINOR_VERSION
              << " by T. Blacklock"
              << std::endl;

    // Loop over every argument passed by shell
    for (int i = 0; i < argc; ++i) {
        Uci::parse(argv[i]);
        // If bench was passed make sure to exit after to comply with Openbench
        if (!std::strcmp(argv[i], "bench"))
            parse("exit");
    }

    // Process commands sent by the gui
    std::string line;

    while (std::getline(std::cin, line)) {
        Uci::parse(line);
    }
}

void Uci::parse(std::string command) {
    // Split the input with a whitespace as delimiter
    std::vector<std::string> args = split(command, ' ');

    // Store the first word as a token
    std::string token = args.at(0);

    // Look through each possibility
    if (token == "uci") {
        uci();
    }
    else if (token == "go") {
        parse_go(command);
    }
    else if (token == "position") {
        parse_position(command);
    }
    else if (token == "setoption") {
        // Find the value from keyword
        std::string opt = find_val_from_key(command, "name");
        std::string val = find_val_from_key(command, "value");
        // Set the option
        parse_option(opt, val);
    }
    else if (token == "ucinewgame") {
        newgame();
    }
    else if (token == "isready") {
        std::cout << "readyok" << std::endl;
    }
    else if (token == "stop") {
        stop();
    }
    else if (token == "eval") {
        std::cout << network.predict(&pos) << std::endl;
    }
    else if (token == "bench") {
        bench();
    }
    else if (token == "d") {
        std::cout << pos << std::endl;
    }
    else if (token == "exit" || token == "quit") {
        stop();
        exit(0);
    }
}

void Uci::parse_go(std::string command) {
    // Stop any ongoing search
    stop();

    // Check a special case when running perft
    if (command.find("perft") != std::string::npos) {
        // Find the depth
        std::string str = find_val_from_key(command, "perft");

        // Set depth default to 1
        Depth d = 1;

        // Ensure the value can be a number
        if (is_number(str)) {
            d = std::stoi(str);
        }

        // Call perft and print the total
        Perft().main_thread(&pos, d, numThreads);

        // Return after perft call
        return;
    }

    // Reset the time manager
    tm.reset();

    // Extract possible keywords from input
    // Total time left
    uint64_t wtime = get_val_from_key<uint64_t>(command, "wtime");
    uint64_t btime = get_val_from_key<uint64_t>(command, "btime");
    uint64_t winc = get_val_from_key<uint64_t>(command, "winc");
    uint64_t binc = get_val_from_key<uint64_t>(command, "binc");
    int mtg = get_val_from_key<int>(command, "movestogo");

    // Max depth
    Depth depth = get_val_from_key<Depth>(command, "depth");

    // Max nodes
    uint64_t nodes = get_val_from_key<uint64_t>(command, "nodes");

    // Time per move
    uint64_t movetime = get_val_from_key<uint64_t>(command, "movetime");

    // Set possible keywords
    // Total time left
    if (wtime || btime || winc || binc || mtg) {
        uint64_t sideTime = pos.side() ? wtime : btime;
        uint64_t sideInc = pos.side() ? winc : binc;
        tm.set_time_limit(sideTime, sideInc, mtg, pos.move_count());
    }
    // Max depth
    if (depth) {
        tm.set_depth_limit(depth);
    }
    // Max nodes
    if (nodes) {
        tm.set_node_limit(nodes);
    }
    // Time per move
    if (movetime) {
        tm.set_move_time_limit(movetime);
    }

    // Start the search
    mainThread = std::thread(&Uci::search, this);
}

void Uci::parse_position(std::string command) {
    // Find the fen and the subsequent moves
    auto fenStr = command.find("fen");
    auto movesStr = command.find("moves");
    auto startposStr = command.find("startpos");

    // Store the moves
    std::string moves;

    // Check for starting position
    if (startposStr != std::string::npos) {
        pos = Position("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", false);
    }

    // Extract the fen
    if (fenStr != std::string::npos) {
        std::string fen = command.substr(fenStr + 4);
        pos = Position(fen, false);
    }

    // Extract the moves
    if (movesStr != std::string::npos) {
        moves = command.substr(movesStr + 6);
    }

    // Prepare loop to make the moves on the board.
    // This has the added bonus of checking the moves legality.
    if (moves.empty()) return;

    // Split the moves using space as a delimiter
    std::vector<std::string> movesVector = split(moves, ' ');

    // Loop through each move
    for (std::string& m : movesVector) {
        // Make sure whitespaces and improper moves are not tested
        if (m.length() < 4) continue;

        // Create the move from string
        Move move = to_move(m);

        // If move is none, return since there was an invalid move
        if (move == Move::none()) return;

        // Do the move otherwise
        pos.do_move<false>(move);
    }
}

void Uci::parse_option(std::string opt, std::string val) {
    // Check for option names
    if (opt == "Threads") {
        numThreads = is_number(val) ? std::stoi(val) : 1;
        s.set_threads(numThreads);
    }
    else if (opt == "Hash") {
        size_t mb = is_number(val) ? std::stoi(val) : 16;
        table.resize(mb);
    }
}

void Uci::newgame() {
    s.clear_thread_data();
    table.clear();
}

void Uci::stop() {
    tm.stop();
    if (mainThread.joinable()) mainThread.join();
}

static const std::string benchPositions[50] = {
    "r3k2r/2pb1ppp/2pp1q2/p7/1nP1B3/1P2P3/P2N1PPP/R2QK2R w KQkq a6 0 14",	
    "4rrk1/2p1b1p1/p1p3q1/4p3/2P2n1p/1P1NR2P/PB3PP1/3R1QK1 b - - 2 24",	
    "r3qbrk/6p1/2b2pPp/p3pP1Q/PpPpP2P/3P1B2/2PB3K/R5R1 w - - 16 42",	
    "6k1/1R3p2/6p1/2Bp3p/3P2q1/P7/1P2rQ1K/5R2 b - - 4 44",	
    "8/8/1p2k1p1/3p3p/1p1P1P1P/1P2PK2/8/8 w - - 3 54",	
    "7r/2p3k1/1p1p1qp1/1P1Bp3/p1P2r1P/P7/4R3/Q4RK1 w - - 0 36",	
    "r1bq1rk1/pp2b1pp/n1pp1n2/3P1p2/2P1p3/2N1P2N/PP2BPPP/R1BQ1RK1 b - - 2 10",	
    "3r3k/2r4p/1p1b3q/p4P2/P2Pp3/1B2P3/3BQ1RP/6K1 w - - 3 87",	
    "2r4r/1p4k1/1Pnp4/3Qb1pq/8/4BpPp/5P2/2RR1BK1 w - - 0 42",	
    "4q1bk/6b1/7p/p1p4p/PNPpP2P/KN4P1/3Q4/4R3 b - - 0 37",	
    "2q3r1/1r2pk2/pp3pp1/2pP3p/P1Pb1BbP/1P4Q1/R3NPP1/4R1K1 w - - 2 34",	
    "1r2r2k/1b4q1/pp5p/2pPp1p1/P3Pn2/1P1B1Q1P/2R3P1/4BR1K b - - 1 37",	
    "r3kbbr/pp1n1p1P/3ppnp1/q5N1/1P1pP3/P1N1B3/2P1QP2/R3KB1R b KQkq b3 0 17",	
    "8/6pk/2b1Rp2/3r4/1R1B2PP/P5K1/8/2r5 b - - 16 42",	
    "1r4k1/4ppb1/2n1b1qp/pB4p1/1n1BP1P1/7P/2PNQPK1/3RN3 w - - 8 29",	
    "8/p2B4/PkP5/4p1pK/4Pb1p/5P2/8/8 w - - 29 68",	
    "3r4/ppq1ppkp/4bnp1/2pN4/2P1P3/1P4P1/PQ3PBP/R4K2 b - - 2 20",	
    "5rr1/4n2k/4q2P/P1P2n2/3B1p2/4pP2/2N1P3/1RR1K2Q w - - 1 49",	
    "1r5k/2pq2p1/3p3p/p1pP4/4QP2/PP1R3P/6PK/8 w - - 1 51",	
    "q5k1/5ppp/1r3bn1/1B6/P1N2P2/BQ2P1P1/5K1P/8 b - - 2 34",	
    "r1b2k1r/5n2/p4q2/1ppn1Pp1/3pp1p1/NP2P3/P1PPBK2/1RQN2R1 w - - 0 22",	
    "r1bqk2r/pppp1ppp/5n2/4b3/4P3/P1N5/1PP2PPP/R1BQKB1R w KQkq - 0 5",	
    "r1bqr1k1/pp1p1ppp/2p5/8/3N1Q2/P2BB3/1PP2PPP/R3K2n b Q - 1 12",	
    "r1bq2k1/p4r1p/1pp2pp1/3p4/1P1B3Q/P2B1N2/2P3PP/4R1K1 b - - 2 19",	
    "r4qk1/6r1/1p4p1/2ppBbN1/1p5Q/P7/2P3PP/5RK1 w - - 2 25",	
    "r7/6k1/1p6/2pp1p2/7Q/8/p1P2K1P/8 w - - 0 32",	
    "r3k2r/ppp1pp1p/2nqb1pn/3p4/4P3/2PP4/PP1NBPPP/R2QK1NR w KQkq - 1 5",	
    "3r1rk1/1pp1pn1p/p1n1q1p1/3p4/Q3P3/2P5/PP1NBPPP/4RRK1 w - - 0 12",	
    "5rk1/1pp1pn1p/p3Brp1/8/1n6/5N2/PP3PPP/2R2RK1 w - - 2 20",	
    "8/1p2pk1p/p1p1r1p1/3n4/8/5R2/PP3PPP/4R1K1 b - - 3 27",	
    "8/4pk2/1p1r2p1/p1p4p/Pn5P/3R4/1P3PP1/4RK2 w - - 1 33",	
    "8/5k2/1pnrp1p1/p1p4p/P6P/4R1PK/1P3P2/4R3 b - - 1 38",	
    "8/8/1p1kp1p1/p1pr1n1p/P6P/1R4P1/1P3PK1/1R6 b - - 15 45",	
    "8/8/1p1k2p1/p1prp2p/P2n3P/6P1/1P1R1PK1/4R3 b - - 5 49",	
    "8/8/1p4p1/p1p2k1p/P2npP1P/4K1P1/1P6/3R4 w - - 6 54",	
    "8/8/1p4p1/p1p2k1p/P2n1P1P/4K1P1/1P6/6R1 b - - 6 59",	
    "8/5k2/1p4p1/p1pK3p/P2n1P1P/6P1/1P6/4R3 b - - 14 63",	
    "8/1R6/1p1K1kp1/p6p/P1p2P1P/6P1/1Pn5/8 w - - 0 67",	
    "1rb1rn1k/p3q1bp/2p3p1/2p1p3/2P1P2N/PP1RQNP1/1B3P2/4R1K1 b - - 4 23",	
    "4rrk1/pp1n1pp1/q5p1/P1pP4/2n3P1/7P/1P3PB1/R1BQ1RK1 w - - 3 22",	
    "r2qr1k1/pb1nbppp/1pn1p3/2ppP3/3P4/2PB1NN1/PP3PPP/R1BQR1K1 w - - 4 12",	
    "2r2k2/8/4P1R1/1p6/8/P4K1N/7b/2B5 b - - 0 55",	
    "6k1/5pp1/8/2bKP2P/2P5/p4PNb/B7/8 b - - 1 44",	
    "2rqr1k1/1p3p1p/p2p2p1/P1nPb3/2B1P3/5P2/1PQ2NPP/R1R4K w - - 3 25",	
    "r1b2rk1/p1q1ppbp/6p1/2Q5/8/4BP2/PPP3PP/2KR1B1R b - - 2 14",	
    "6r1/5k2/p1b1r2p/1pB1p1p1/1Pp3PP/2P1R1K1/2P2P2/3R4 w - - 1 36",	
    "rnbqkb1r/pppppppp/5n2/8/2PP4/8/PP2PPPP/RNBQKBNR b KQkq c3 0 2",	
    "2rr2k1/1p4bp/p1q1p1p1/4Pp1n/2PB4/1PN3P1/P3Q2P/2RR2K1 w - f6 0 20",	
    "3br1k1/p1pn3p/1p3n2/5pNq/2P1p3/1PN3PP/P2Q1PB1/4R1K1 w - - 0 23",	
    "2r2b2/5p2/5k2/p1r1pP2/P2pB3/1P3P2/K1P3R1/7R w - - 23 93"
};

void Uci::bench() {
    uint64_t nodes = 0;
    uint64_t time = 0;

    for (int i = 0; i < 50; ++i) {
        // Initialize a position with the given bench position
        Position benchPos(benchPositions[i], false);
        // Setup a new time manager and limit to a depth of 12
        TimeManager benchtm;
        benchtm.set_depth_limit(12);
        // Call a new search
        s.search(&benchPos, &benchtm);

        // Update search nodes and time
        nodes += s.total_nodes();
        time += benchtm.elapsed();

        // Clear the search and hashtable
        s.clear_thread_data();
        table.clear();
    }

    // Print overall stats
    std::cout << std::endl;
    std::cout << "-- Bench Results --" << std::endl;
    std::cout << nodes << " nodes" << std::endl; 
    std::cout << 1000 * nodes / (time + 1) << " nps" << std::endl;
}

void Uci::quit() {
    // Stop the search
    stop();
    // Deallocate the transposition table
    table.dealloc();
}

void Uci::search() {
    Move m = s.search(&pos, &tm);
    std::cout << "bestmove " << from_move(m, pos.is_chess960()) << std::endl;
}

}
