#include <iostream>

#include "misc.hpp"
#include "uci.hpp"
#include "position.hpp"
#include "bitboard.hpp"
#include "movegen.hpp"
#include "nn/layers.hpp"

using namespace Stella;

int main(int argc, char* argv[]) {
    Bitboards::init();
    Position::init();
    Features::init();

    Uci uci;
    uci.loop(argc, argv);
}
