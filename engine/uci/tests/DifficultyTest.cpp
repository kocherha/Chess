#include "chess/Difficulty.h"

#include <gtest/gtest.h>

#include <map>

using namespace chess;

namespace {
std::map<std::string, std::string> asMap(const std::vector<std::pair<std::string, std::string>>& options) {
    return {options.begin(), options.end()};
}
}  // namespace

TEST(Difficulty, LevelsGetHarderAllTheWayUp) {
    const auto& table = levels();
    ASSERT_GE(table.size(), 3u);

    for (std::size_t i = 0; i < table.size(); ++i) {
        EXPECT_EQ(table[i].number, static_cast<int>(i) + 1);
        EXPECT_GT(table[i].moveTimeMs, 0);
        EXPECT_GE(table[i].randomMoveShare, 0.0);
        EXPECT_LT(table[i].randomMoveShare, 0.5);  // still mostly chess
    }

    // Each step up is stronger in at least one way and weaker in none: fewer
    // random moves, or a higher rating, or no limit at all.
    for (std::size_t i = 1; i < table.size(); ++i) {
        const Level& easier = table[i - 1];
        const Level& harder = table[i];
        EXPECT_LE(harder.randomMoveShare, easier.randomMoveShare);
        if (harder.fullStrength())
            continue;
        ASSERT_FALSE(easier.fullStrength());
        EXPECT_GE(harder.elo, easier.elo);
        EXPECT_TRUE(harder.elo > easier.elo || harder.randomMoveShare < easier.randomMoveShare);
    }

    // Only the last one is unrestricted, and random moves stop before the
    // engine's own scale starts to climb.
    EXPECT_TRUE(table.back().fullStrength());
    for (const Level& level : table) {
        if (level.randomMoveShare > 0.0) {
            EXPECT_EQ(level.elo, table.front().elo);
        }
    }
}

TEST(Difficulty, LookupClamps) {
    EXPECT_EQ(levelByNumber(-5).number, 1);
    EXPECT_EQ(levelByNumber(4).number, 4);
    EXPECT_EQ(levelByNumber(1000).number, levels().back().number);
    EXPECT_EQ(levelByNumber(defaultLevelNumber()).number, defaultLevelNumber());
}

TEST(Difficulty, TheLevelsAreStopsOnTheScale) {
    // Each level is somewhere on the scale, in order, and what the scale says
    // about that place is the level itself — rating, time, random moves, all.
    ASSERT_GT(strengthCount(), static_cast<int>(levels().size()));
    EXPECT_EQ(strengthOfLevel(levels().front().number), 0);
    EXPECT_EQ(strengthOfLevel(levels().back().number), strengthCount() - 1);
    EXPECT_EQ(defaultStrength(), strengthOfLevel(defaultLevelNumber()));

    int before = -1;
    for (const Level& level : levels()) {
        const int strength = strengthOfLevel(level.number);
        EXPECT_GT(strength, before) << "level " << level.number;
        before = strength;

        const Level there = levelAtStrength(strength);
        EXPECT_TRUE(there.exact) << "level " << level.number;
        EXPECT_EQ(there.number, level.number);
        EXPECT_EQ(there.elo, level.elo);
        EXPECT_EQ(there.moveTimeMs, level.moveTimeMs);
        EXPECT_DOUBLE_EQ(there.randomMoveShare, level.randomMoveShare);
    }
}

TEST(Difficulty, TheScaleGetsHarderOneSmallStepAtATime) {
    for (int strength = 1; strength < strengthCount(); ++strength) {
        const Level easier = levelAtStrength(strength - 1);
        const Level harder = levelAtStrength(strength);

        // Never easier in any way...
        EXPECT_GE(harder.number, easier.number) << strength;
        EXPECT_GE(harder.moveTimeMs, easier.moveTimeMs) << strength;
        EXPECT_LE(harder.randomMoveShare, easier.randomMoveShare) << strength;
        ASSERT_FALSE(easier.fullStrength()) << strength;  // only the very last is

        // ...and harder in exactly one: a random move in a hundred fewer, ten
        // points of rating more, or the limit taken off.
        if (harder.fullStrength()) {
            EXPECT_EQ(strength, strengthCount() - 1);
        } else if (easier.randomMoveShare > 0.0) {
            EXPECT_EQ(harder.elo, easier.elo) << strength;
            EXPECT_NEAR(easier.randomMoveShare - harder.randomMoveShare, 0.01, 1e-9) << strength;
        } else {
            EXPECT_EQ(harder.elo, easier.elo + 10) << strength;
        }
    }

    // Random moves are a handicap for the engine at its floor, nowhere else.
    for (int strength = 0; strength < strengthCount(); ++strength) {
        const Level level = levelAtStrength(strength);
        if (level.randomMoveShare > 0.0) {
            EXPECT_EQ(level.elo, levels().front().elo) << strength;
        }
    }
}

TEST(Difficulty, BetweenTwoLevels) {
    // Halfway from level 4 (1600, 0.4 s) to level 5 (1900, 0.5 s).
    const Level level = levelAtStrength(strengthForElo(1750));
    EXPECT_EQ(level.elo, 1750);
    EXPECT_EQ(level.number, 4);  // the one it has reached
    EXPECT_FALSE(level.exact);
    EXPECT_EQ(level.moveTimeMs, 450);
    EXPECT_DOUBLE_EQ(level.randomMoveShare, 0.0);
    EXPECT_EQ(asMap(engineOptions(level, 1320, 3190, 8)).at("UCI_Elo"), "1750");

    // And one step short of level 2: it still counts as level 1.
    const Level beginner = levelAtStrength(strengthOfLevel(2) - 1);
    EXPECT_EQ(beginner.number, 1);
    EXPECT_FALSE(beginner.exact);
    EXPECT_NEAR(beginner.randomMoveShare, 0.16, 1e-9);
}

TEST(Difficulty, TheScaleClamps) {
    EXPECT_EQ(levelAtStrength(-7).number, 1);
    EXPECT_TRUE(levelAtStrength(-7).exact);
    EXPECT_TRUE(levelAtStrength(strengthCount() + 100).fullStrength());

    // A rating the engine does not go down to is its floor, played straight.
    const Level floor = levelAtStrength(strengthForElo(100));
    EXPECT_EQ(floor.elo, 1320);
    EXPECT_DOUBLE_EQ(floor.randomMoveShare, 0.0);

    // One it does not go up to is its ceiling, which is still a rating.
    const Level ceiling = levelAtStrength(strengthForElo(9000));
    EXPECT_EQ(ceiling.elo, 3190);
    EXPECT_FALSE(ceiling.fullStrength());
    EXPECT_EQ(strengthForElo(9000), strengthCount() - 2);

    // In between, the nearest step.
    EXPECT_EQ(levelAtStrength(strengthForElo(1754)).elo, 1750);
    EXPECT_EQ(levelAtStrength(strengthForElo(1755)).elo, 1760);
}

TEST(Difficulty, LimitedLevelsAskForARating) {
    const auto options = asMap(engineOptions(levelByNumber(5), 1320, 3190, 8));
    EXPECT_EQ(options.at("UCI_LimitStrength"), "true");
    EXPECT_EQ(options.at("UCI_Elo"), "1900");
    EXPECT_EQ(options.at("Threads"), "1");
}

TEST(Difficulty, RatingIsClampedToWhatTheEngineOffers) {
    // An engine with a narrower scale than Stockfish's.
    EXPECT_EQ(asMap(engineOptions(levelByNumber(1), 1500, 2400, 8)).at("UCI_Elo"), "1500");
    EXPECT_EQ(asMap(engineOptions(levelByNumber(7), 1500, 2400, 8)).at("UCI_Elo"), "2400");
    // One that declared no range gets the number as it is.
    EXPECT_EQ(asMap(engineOptions(levelByNumber(4), 0, 0, 8)).at("UCI_Elo"), "1600");
}

TEST(Difficulty, TopLevelIsUnrestricted) {
    const auto options = asMap(engineOptions(levels().back(), 1320, 3190, 8));
    EXPECT_EQ(options.at("UCI_LimitStrength"), "false");
    EXPECT_EQ(options.count("UCI_Elo"), 0u);
    EXPECT_EQ(options.at("Threads"), "4");

    EXPECT_EQ(asMap(fullStrengthOptions(2)).at("Threads"), "1");
    EXPECT_EQ(asMap(fullStrengthOptions(0)).at("Threads"), "1");   // unknown hardware
    EXPECT_EQ(asMap(fullStrengthOptions(64)).at("Threads"), "4");
}

TEST(Difficulty, SwitchingLevelsLeavesNothingBehind) {
    // Going down from the top level must turn the limit back on and the extra
    // threads back off — every option the top level sets, a limited one resets.
    const auto strong  = asMap(engineOptions(levels().back(), 1320, 3190, 8));
    const auto limited = asMap(engineOptions(levelByNumber(2), 1320, 3190, 8));
    for (const auto& [name, value] : strong)
        EXPECT_EQ(limited.count(name), 1u) << name;
}

TEST(Difficulty, TheAnalystIsStrongAndQuiet) {
    const auto options = asMap(analysisOptions());
    EXPECT_EQ(options.at("UCI_LimitStrength"), "false");
    EXPECT_EQ(options.at("Skill Level"), "20");
    EXPECT_EQ(options.at("Threads"), "1");
    EXPECT_EQ(options.at("MultiPV"), "2");  // the runner-up too: marks need it
}
