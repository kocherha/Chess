// The Stockfish the game's engines run: where it is, and a log of what is
// said to it when one is asked for.
#pragma once

#include "chess/Engine.h"

#include <string>

namespace stockfish {

// chess::findEngine, with the path the build found Stockfish at as the last
// place but one to look. Empty if there is none anywhere.
std::string path();

// CHESS_UCI_LOG=1: every line to and from `engine` goes to stderr, after
// `who` — "uci " for the opponent, "eval" for the engine that watches,
// "puzzle" for the problems mode's own.
void traceIfAsked(chess::Engine& engine, const char* who);

}  // namespace stockfish
