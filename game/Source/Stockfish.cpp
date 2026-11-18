#include "Stockfish.h"

#include <cstdio>
#include <cstdlib>

// The path the build found Stockfish at, when it found one.
#ifndef CHESS_ENGINE_FALLBACK
#    define CHESS_ENGINE_FALLBACK ""
#endif

namespace stockfish {

std::string path() {
    return chess::findEngine(CHESS_ENGINE_FALLBACK);
}

void traceIfAsked(chess::Engine& engine, const char* who) {
    static const bool asked = std::getenv("CHESS_UCI_LOG") != nullptr;
    if (!asked)
        return;
    engine.setTrace([who](char direction, const std::string& line) {
        std::fprintf(stderr, "%s %c %s\n", who, direction, line.c_str());
    });
}

}  // namespace stockfish
