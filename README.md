## Overview
Stella is a cutting edge UCI compatible chess engine written in C++. Heavily inspired by Stockfish, Koivisto, Berserk and more.
Stella is able to analyze any legal standard or Chess960 chess position and determine a numeric evaluation and the optimal next move.

Stella does not include a GUI, and instead can comminicate with seperately developed GUI's using the Universal Chess Interface.
Popular and free GUI's to use include [Cutechess][cutechess-link] and [Arena][arena-link]. As another option, Stella can be entirely
ran through command line using the [UCI][uci-link] protocol.

## Options
Suported UCI options include:
- Hash
- Threads
- MoveOverhead

To structure a command to change a UCI option, input the following into the engine:

```
setoption name "option name" value "value to set"
```

An example to set the Hash table of the engine to 64MB would be as follows:

```
setoption name Hash value 64
```

## Compiling
Stella currently only supports builds for 64-bit Windows and Linux.

The makefile is located in the `src` folder of the code, and it is recommended to run `make help`
to see build targets and options that can be used.

An example that will compile an executable native to your system is as follows:
```
cd src
make -j build
```
It is strongly recommended to create your own optimized executable by building the target:
```
cd src
make -j pgo
```

[cutechess-link]: https://github.com/cutechess/cutechess
[arena-link]: https://www.playwitharena.de/
[uci-link]: https://page.mi.fu-berlin.de/block/uci.htm
