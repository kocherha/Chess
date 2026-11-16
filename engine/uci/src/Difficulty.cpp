#include "chess/Difficulty.h"

#include <algorithm>
#include <cmath>

namespace chess {

namespace {

// Search threads for the unrestricted level. Half the machine, at most four:
// more would make it stronger still, but it already wins every game, and the
// difference would only be heard in the fan.
unsigned strongThreads(unsigned hardwareThreads) {
    return std::clamp(hardwareThreads / 2, 1u, 4u);
}

// The scale the levels stand on. The ratings are Stockfish's own range for
// UCI_Elo; an engine with a narrower one is asked for the nearest it has
// (engineOptions).
constexpr int kFloorElo   = 1320;
constexpr int kCeilingElo = 3190;
constexpr int kEloStep    = 10;

// Strengths on which the engine is asked for a rating and nothing else.
constexpr int kRatedCount = (kCeilingElo - kFloorElo) / kEloStep + 1;

int hundredths(double share) {
    return static_cast<int>(std::lround(share * 100.0));
}

// Strengths below the engine's floor: one for every hundredth of random
// moves, from the first level's share down to a single one in a hundred.
int handicapCount() {
    return hundredths(levels().front().randomMoveShare);
}

}  // namespace

const std::vector<Level>& levels() {
    // Thinking time grows with the level for two reasons. The lower levels do
    // not need it — a deliberately weakened engine does not get better with
    // time, it only makes the player wait. And the upper ones do: the rating
    // Stockfish aims for assumes it is given room to think.
    //
    // Every level is a place on the scale (see the header): a rating here is
    // a whole number of steps above the floor, a share of random moves a
    // whole number of hundredths.
    static const std::vector<Level> table = {
        {1, 1320, 150, 0.35},
        {2, 1320, 200, 0.15},
        {3, 1320, 300, 0.0},
        {4, 1600, 400, 0.0},
        {5, 1900, 500, 0.0},
        {6, 2200, 700, 0.0},
        {7, 2600, 1000, 0.0},
        {8, 0, 2000, 0.0},
    };
    return table;
}

int defaultLevelNumber() {
    return 3;
}

const Level& levelByNumber(int number) {
    const auto& table = levels();
    const int index   = std::clamp(number, 1, static_cast<int>(table.size())) - 1;
    return table[static_cast<std::size_t>(index)];
}

int strengthCount() {
    return handicapCount() + kRatedCount + 1;
}

int strengthOfLevel(int number) {
    const Level& level = levelByNumber(number);
    if (level.fullStrength())
        return strengthCount() - 1;
    if (level.randomMoveShare > 0.0)
        return handicapCount() - hundredths(level.randomMoveShare);
    return handicapCount() + (std::clamp(level.elo, kFloorElo, kCeilingElo) - kFloorElo) / kEloStep;
}

int defaultStrength() {
    return strengthOfLevel(defaultLevelNumber());
}

Level levelAtStrength(int strength) {
    strength = std::clamp(strength, 0, strengthCount() - 1);

    // The level it has reached, and the next one up if there is one.
    const auto& table = levels();
    std::size_t below = 0;
    while (below + 1 < table.size() && strengthOfLevel(table[below + 1].number) <= strength)
        ++below;
    const Level& reached = table[below];
    const int    from    = strengthOfLevel(reached.number);
    if (strength == from)
        return reached;

    Level level;
    level.number = reached.number;
    level.exact  = false;

    if (strength < handicapCount()) {
        level.elo             = kFloorElo;
        level.randomMoveShare = (handicapCount() - strength) / 100.0;
    } else {
        // The last strength is the last level, and that one was returned
        // above: everything that gets here has a rating.
        level.elo = kFloorElo + (strength - handicapCount()) * kEloStep;
    }

    // Time grows evenly from one level to the next, to the hundredth of a
    // second: nobody can tell 437 ms from 440, and the round number is the
    // one that can be written down.
    const Level& next = table[below + 1];
    const double way  = static_cast<double>(strength - from) / (strengthOfLevel(next.number) - from);
    const double time = reached.moveTimeMs + way * (next.moveTimeMs - reached.moveTimeMs);
    level.moveTimeMs  = static_cast<int>(std::lround(time / 10.0)) * 10;
    return level;
}

int strengthForElo(int elo) {
    const int steps = (std::clamp(elo, kFloorElo, kCeilingElo) - kFloorElo + kEloStep / 2) / kEloStep;
    return handicapCount() + std::min(steps, kRatedCount - 1);
}

std::vector<std::pair<std::string, std::string>> engineOptions(const Level& level, int eloMin,
                                                              int eloMax, unsigned hardwareThreads) {
    if (level.fullStrength())
        return fullStrengthOptions(hardwareThreads);

    int elo = level.elo;
    if (eloMin > 0 && eloMax >= eloMin)
        elo = std::clamp(elo, eloMin, eloMax);

    return {
        {"Threads", "1"},
        {"Skill Level", "20"},
        {"UCI_LimitStrength", "true"},
        {"UCI_Elo", std::to_string(elo)},
    };
}

std::vector<std::pair<std::string, std::string>> fullStrengthOptions(unsigned hardwareThreads) {
    return {
        {"Threads", std::to_string(strongThreads(hardwareThreads))},
        {"Skill Level", "20"},
        {"UCI_LimitStrength", "false"},
    };
}

std::vector<std::pair<std::string, std::string>> analysisOptions() {
    return {
        {"Threads", "1"},
        {"Skill Level", "20"},
        {"UCI_LimitStrength", "false"},
        // The best move and the runner-up: how far apart they are is what
        // tells an only move from one of several good ones.
        {"MultiPV", "2"},
    };
}

}  // namespace chess
