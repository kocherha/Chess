// What happened, one line at a time, for whoever is watching from outside.
//
// CHESS_EVENT_LOG=1 prints a line on the error stream for each thing a player
// would notice — a move on the board, a move taken back, a problem solved, the
// position in the editor changed. It is how tools/ui-checks, which drive the
// game with a pretend mouse, find out whether what they did had the effect it
// should; nothing in the game reads it. Off, it costs a test of a flag and
// the short string a caller puts together — a few times per move at most.
#pragma once

#include <cstdio>
#include <cstdlib>
#include <string>

namespace eventlog {

inline bool enabled() {
    static const bool on = std::getenv("CHESS_EVENT_LOG") != nullptr;
    return on;
}

// "move e2e4" → "event: move e2e4"
inline void note(const std::string& what) {
    if (!enabled())
        return;
    std::fprintf(stderr, "event: %s\n", what.c_str());
    std::fflush(stderr);
}

}  // namespace eventlog
