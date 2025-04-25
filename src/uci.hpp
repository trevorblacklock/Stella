#ifndef UCI_H_INCLUDED
#define UCI_H_INCLUDED

#include "position.hpp"
#include "search.hpp"
#include "timing.hpp"
#include "nn/evaluate.hpp"

#include <string>

namespace Stella {

class Uci {

private:
    std::thread mainThread;
    Search s;
    TimeManager tm;
    Position pos = Position("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", false);
    Network::Evaluator network;
    int numThreads = 1;

public:
    Uci();
    ~Uci();

    // Algebraic move notation to move
    const Move to_move(std::string move);

    // Main function to call from main to loop over arguments read from command line.
    void loop(int argc, char* argv[]);

    // Function to parse arguments sent from the command line, entered through Uci::loop().
    void parse(std::string cmd);

    // Function to parse a command beginning in "go", for example "go depth 5".
    void parse_go(std::string cmd);

    // Function to parse a command beginning in "position", for example "position startpos".
    void parse_position(std::string cmd);

    // Function to parse a command beginning in "setoption", for example "setoption name Hash value 16".
    void parse_option(std::string opt, std::string val);

    // Start a new game
    void newgame();

    // Stop the search.
    void stop();

    // Write Uci information to stdout.
    void uci();

    // Bench function to run a benchmark, used for profile guided optimization and performace testing.
    void bench();

    // Quit the program.
    void quit();

    // Initiate a search.
    void search();
};

}

#endif
