#include "uci.hpp"
#include "perft.hpp"
#include "types.hpp"
#include "bitboard.hpp"
#include "movegen.hpp"
#include "evaluate.hpp"
#include "position.hpp"
#include "tt.hpp"
#include "misc.hpp"
#include "search.hpp"
#include "timing.hpp"

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
    else if (token == "isready") {
        std::cout << "readyok" << std::endl;
    }
    else if (token == "stop") {
        stop();
    }
    else if (token == "eval") {
        std::cout << Evaluate::evaluate(&pos) << std::endl;
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

void Uci::stop() {
    tm.stop();
    if (mainThread.joinable()) mainThread.join();
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
