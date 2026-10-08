// How strong the opponent is allowed to be.
//
// Stockfish at full strength beats every human alive, so "difficulty" means
// holding it back, and it offers one honest way to do that: UCI_LimitStrength
// with UCI_Elo, a rating it was calibrated to play at. Its floor is around
// 1320, though, which is already a decent club player — far too much for
// someone who has just learned how the knight moves. Below the floor the game
// adds a handicap of its own: every so often the move is not the engine's at
// all, but a random legal one. A beginner's opponent that sometimes leaves a
// piece hanging is exactly what a beginner's opponent should be.
#pragma once

#include <string>
#include <utility>
#include <vector>

namespace chess {

struct Level {
    int number = 0;  // 1-based, as shown to the player

    // The rating the engine is asked to play at; 0 means no limit at all.
    int elo = 0;

    // How long it may think about a move.
    int moveTimeMs = 0;

    // The share of moves (0..1) that are picked at random instead of asked of
    // the engine. Non-zero only below the engine's own floor.
    double randomMoveShare = 0.0;

    bool fullStrength() const { return elo == 0; }
};

// The levels on offer, weakest first.
const std::vector<Level>& levels();

// The level to start with: someone who knows the rules but is not a club
// player should be able to win some and lose some.
int defaultLevelNumber();

// Clamps to an existing level.
const Level& levelByNumber(int number);

// The engine options that make it play at `level`, given the Elo range it
// declared (pass 0, 0 if it declared none) and how many hardware threads the
// machine has. Always a complete set, so that switching level in either
// direction leaves nothing behind from the previous one.
std::vector<std::pair<std::string, std::string>> engineOptions(const Level& level, int eloMin,
                                                              int eloMax, unsigned hardwareThreads);

// Full strength, on as many threads as it is polite to take.
std::vector<std::pair<std::string, std::string>> fullStrengthOptions(unsigned hardwareThreads);

// Options for the engine that only watches: full strength, so that what it
// says about a position is true whatever level is being played, but on a
// single thread — it runs after every move and must not be what the fan is
// for. It reports its two best moves, which judging a move needs (Judge.h).
std::vector<std::pair<std::string, std::string>> analysisOptions();

}  // namespace chess
