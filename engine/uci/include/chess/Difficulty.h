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
    // 1-based, as shown to the player. For a strength between two levels (see
    // the scale below) this is the level it has reached: the one below it.
    int number = 0;

    // The rating the engine is asked to play at; 0 means no limit at all.
    int elo = 0;

    // How long it may think about a move.
    int moveTimeMs = 0;

    // The share of moves (0..1) that are picked at random instead of asked of
    // the engine. Non-zero only below the engine's own floor.
    double randomMoveShare = 0.0;

    // Is this one of the levels themselves, rather than somewhere between two?
    bool exact = true;

    bool fullStrength() const { return elo == 0; }
};

// The levels on offer, weakest first.
const std::vector<Level>& levels();

// The level to start with: someone who knows the rules but is not a club
// player should be able to win some and lose some.
int defaultLevelNumber();

// Clamps to an existing level.
const Level& levelByNumber(int number);

// --- the scale between the levels ---------------------------------------------
//
// Eight steps are few: the one that is too easy and the one that is too hard
// are often neighbours. So the levels are stops on a longer scale, and the
// opponent can be put anywhere on it. From the weak end it runs through
//
//   - the engine at its floor with some of its moves replaced by random ones:
//     as many as the first level has, then one in a hundred fewer at a time,
//     down to none;
//   - the engine's own ratings, from its floor to its ceiling, ten points at
//     a time;
//   - no limit at all.
//
// A place on the scale is a "strength": a whole number from 0, the first
// level, to strengthCount() - 1, the last. Thinking time follows along,
// growing evenly from each level to the next.

// How many strengths there are.
int strengthCount();

// Where a level stands on the scale. Clamps to an existing level.
int strengthOfLevel(int number);

// The strength to start with: that of the default level.
int defaultStrength();

// What the opponent is at this strength. Clamps to the scale.
Level levelAtStrength(int strength);

// The strength at which the engine is asked for this rating, or the nearest
// there is: below its floor that is the floor itself, with no random moves,
// and above its ceiling the ceiling — not "no limit", which is not a rating.
int strengthForElo(int elo);

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
