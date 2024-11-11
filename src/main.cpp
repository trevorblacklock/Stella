#include <iostream>

#include "misc.hpp"
#include "uci.hpp"
#include "position.hpp"
#include "bitboard.hpp"
#include "evaluate.hpp"
#include "movegen.hpp"

using namespace Stella;

int main(int argc, char* argv[]) {
    Bitboards::init();
    Position::init();
    Evaluate::init();

    Uci uci;
    uci.loop(argc, argv);
}
