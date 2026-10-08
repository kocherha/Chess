#include "chess/Difficulty.h"

#include <algorithm>

namespace chess {

namespace {

// Search threads for the unrestricted level. Half the machine, at most four:
// more would make it stronger still, but it already wins every game, and the
// difference would only be heard in the fan.
unsigned strongThreads(unsigned hardwareThreads) {
    return std::clamp(hardwareThreads / 2, 1u, 4u);
}

}  // namespace

const std::vector<Level>& levels() {
    // Thinking time grows with the level for two reasons. The lower levels do
    // not need it — a deliberately weakened engine does not get better with
    // time, it only makes the player wait. And the upper ones do: the rating
    // Stockfish aims for assumes it is given room to think.
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
    };
}

}  // namespace chess
