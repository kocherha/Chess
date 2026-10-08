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
