## Overview
Luna is a cutting edge UCI compatible chess engine written in C++. Originally derived from Stockfish and Koivisto, 
this engine is able to analyze any legal chess position and determine a numeric evaluation and the optimal next move.

Luna does not include a GUI, and instead can comminicate with seperately developed GUI's using the Universal Chess Interface.
Popular and free GUI's to use include [Cutechess][cutechess-link] and [Arena][arena-link].

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
Luna currently only supports builds for 64-bit Windows and Linux. 

The makefile is located in the `src` folder of the code, and it is recommended to run `make help`
to see build targets and options that can be used.

An example that will compile an executable native to your system is as follows:
```
cd src
make -j pgo
```

To compile with Windows it is recommended to use [MSYS2][msys-link] and mingw-w64 as other
compilation methods on Windows have not been tested or verified.

[cutechess-link]: https://github.com/cutechess/cutechess
[arena-link]: https://www.playwitharena.de/
[msys-link]: https://www.msys2.org/
