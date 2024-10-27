#ifndef UCI_H_INCLUDED
#define UCI_H_INCLUDED

#include "position.hpp"

#include <string>

namespace Stella {

namespace Uci {

const Move to_move(std::string move, Position pos);

// Main function to call from main to loop over arguments read from command line.
void loop(int argc, char* argv[]);

// Function to parse arguments sent from the command line, entered through Uci::loop().
void parse(std::string cmd, Position &pos);

// Function to parse a command beginning in "go", for example "go depth 5".
void parse_go(std::string cmd, Position& pos);

// Function to parse a command beginning in "position", for example "position startpos".
void parse_position(std::string cmd, Position &pos);

// Function to parse a command beginning in "setoption", for example "setoption name Hash value 16".
void parse_option(std::string opt, std::string val);

// Stop the search.
void stop();

// Write Uci information to stdout.
void uci();

// Function to return the static evaluation of a position.
void eval();

// Bench function to run a benchmark, used for profile guided optimization and performace testing.
void bench();

// Quit the program.
void quit();

}
}

#endif
