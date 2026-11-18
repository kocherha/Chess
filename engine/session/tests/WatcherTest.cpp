#include "chess/Watcher.h"

#include <gtest/gtest.h>

using namespace chess;
using Kind = Standing::Kind;

namespace {

Game play(std::initializer_list<const char*> moves) {
    Game game;
    for (const char* move : moves)
        EXPECT_TRUE(game.playUci(move)) << move;
    return game;
}

// A watcher with an engine, already following `game`.
Watcher watching(const Game& game) {
    Watcher watcher;
    watcher.setEngineUsable(true);
    watcher.newGame();
    watcher.follow(game);
    return watcher;
}

SearchRequest asked(Watcher& watcher) {
    auto work = watcher.toEngine();
    EXPECT_TRUE(work.search.has_value()) << "the watcher asked the engine nothing";
    return work.search.value_or(SearchRequest{});
}

SearchResult said(const SearchRequest& request, int depth, int cp, const char* best = "") {
    SearchResult result;
    result.tag      = request.tag;
    result.depth    = depth;
    result.scoreCp  = cp;
    result.bestMove = best;
    result.pvMove   = best;
    return result;
}

}  // namespace

TEST(Watcher, LooksAtThePositionOnTheBoardFirst) {
    const Game game = play({"e2e4"});
    Watcher watcher = watching(game);
    const SearchRequest request = asked(watcher);
    EXPECT_EQ(request.position, game.uciPosition());
    EXPECT_EQ(request.depth, Watcher::kBoardDepth);
    EXPECT_EQ(request.moveTimeMs, Watcher::kBoardMs);
    EXPECT_GT(request.tag, 0);
}

TEST(Watcher, TheBarIsWhitesViewAndIgnoresShallowNoise) {
    Watcher watcher = watching(play({"e2e4"}));  // black to move
    const SearchRequest request = asked(watcher);

    watcher.progress(said(request, Watcher::kShowFromDepth - 1, 500));
    EXPECT_FALSE(watcher.takeStanding().has_value());

    watcher.progress(said(request, Watcher::kShowFromDepth, -30));  // black is 0.3 worse
    const auto standing = watcher.takeStanding();
    ASSERT_TRUE(standing.has_value());
    EXPECT_EQ(standing->kind, Kind::Centipawns);
    EXPECT_EQ(standing->value, 30);  // white is 0.3 better
    EXPECT_FALSE(watcher.takeStanding().has_value());  // once
}

TEST(Watcher, AHintAskedForEarlyComesWithTheAnswer) {
    Watcher watcher = watching(Game());
    const SearchRequest request = asked(watcher);
    watcher.wantHint();
    EXPECT_TRUE(watcher.hintWanted());
    EXPECT_FALSE(watcher.takeHint().has_value());

    watcher.answer(said(request, 22, 25, "e2e4"));
    EXPECT_EQ(watcher.takeHint(), std::optional<std::string>("e2e4"));
    EXPECT_FALSE(watcher.hintWanted());
}

TEST(Watcher, AHintAskedForLaterComesAtOnce) {
    Watcher watcher = watching(Game());
    watcher.answer(said(asked(watcher), 22, 25, "d2d4"));
    watcher.wantHint();
    EXPECT_EQ(watcher.takeHint(), std::optional<std::string>("d2d4"));
}

// The invariant a hint relies on: until the board is done, nothing else is
// asked. A position left early is gone back over only after that.
TEST(Watcher, GoesBackOverAPositionLeftEarlyOnlyOnceTheBoardIsDone) {
    Watcher watcher = watching(play({"e2e4"}));
    const SearchRequest first = asked(watcher);

    // Black answers before the engine has finished with the position.
    const Game game = play({"e2e4", "e7e5"});
    watcher.follow(game);
    auto work = watcher.toEngine();
    EXPECT_TRUE(work.cancel);
    ASSERT_TRUE(work.search.has_value());
    EXPECT_EQ(work.search->position, game.uciPosition());  // the board, not the one left

    watcher.wantHint();
    watcher.answer(said(first, 22, 30, "e7e5"));  // too late: withdrawn
    EXPECT_FALSE(watcher.takeHint().has_value());

    watcher.answer(said(*work.search, 22, 25, "g1f3"));
    EXPECT_EQ(watcher.takeHint(), std::optional<std::string>("g1f3"));

    // Now, and only now, the position left early.
    const SearchRequest review = asked(watcher);
    EXPECT_EQ(review.position, play({"e2e4"}).uciPosition());
}

TEST(Watcher, APositionSeenBeforeIsKnownAtOnce) {
    Watcher watcher = watching(play({"e2e4"}));
    watcher.answer(said(asked(watcher), 22, -40, "c7c5"));
    watcher.takeStanding();

    watcher.follow(play({"e2e4", "c7c5"}));
    watcher.toEngine();
    watcher.follow(play({"e2e4"}));  // taken back
    const auto work = watcher.toEngine();
    EXPECT_FALSE(work.search.has_value() && work.search->position == play({"e2e4"}).uciPosition());
    const auto standing = watcher.takeStanding();
    ASSERT_TRUE(standing.has_value());
    EXPECT_EQ(standing->value, 40);
}

TEST(Watcher, AFinishedGameStandsAsItsResult) {
    // Fool's mate: black wins.
    Watcher watcher = watching(play({"f2f3", "e7e5", "g2g4", "d8h4"}));
    const auto standing = watcher.takeStanding();
    ASSERT_TRUE(standing.has_value());
    EXPECT_EQ(standing->kind, Kind::BlackWon);
    // Nothing to ask about the board; the moves that led there may still be
    // gone back over.
    const auto work = watcher.toEngine();
    if (work.search) {
        EXPECT_NE(work.search->position, play({"f2f3", "e7e5", "g2g4", "d8h4"}).uciPosition());
    }
}

TEST(Watcher, LentAwayItKeepsWhatItHadAndAsksNothing) {
    const Game game = play({"e2e4"});
    Watcher watcher = watching(game);
    const SearchRequest request = asked(watcher);
    watcher.progress(said(request, 12, -20, "e7e5"));

    watcher.lend();
    const auto* found = watcher.review().found(game.uciPosition());
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->depth, 12);
    EXPECT_FALSE(found->complete);
    const auto work = watcher.toEngine();
    EXPECT_FALSE(work.cancel);  // the borrower cancels it
    EXPECT_FALSE(work.search.has_value());

    watcher.answer(said(request, 22, -20, "e7e5"));  // not ours any more
    EXPECT_FALSE(watcher.review().found(game.uciPosition())->complete);

    // Back: the board again.
    watcher.follow(game);
    EXPECT_EQ(asked(watcher).position, game.uciPosition());
}

TEST(Watcher, AnEngineComingUpLooksAtTheBoardButOnlyOnceThereIsAGame) {
    Watcher watcher;
    watcher.setEngineUsable(true);  // no game yet: nothing to look at
    EXPECT_FALSE(watcher.toEngine().search.has_value());

    watcher.setEngineUsable(false);
    watcher.newGame();
    watcher.follow(Game());
    EXPECT_FALSE(watcher.toEngine().search.has_value());  // no engine
    watcher.setEngineUsable(true);
    EXPECT_EQ(asked(watcher).position, Game().uciPosition());
}

TEST(Watcher, AMoveMarkedOnceBothSidesOfItAreKnown) {
    // 1. e4: both positions looked at deeply enough, the move gets its mark
    // (None or better: e4 is fine) and the marks are reported changed.
    Watcher watcher = watching(Game());
    watcher.answer(said(asked(watcher), 22, 30, "e2e4"));
    watcher.takeMarksChanged();
    watcher.follow(play({"e2e4"}));
    watcher.answer(said(asked(watcher), 22, -30, "e7e5"));
    EXPECT_TRUE(watcher.takeMarksChanged());
    EXPECT_EQ(watcher.review().marks().size(), 1u);
}
