#include "chess/GameReview.h"

#include <gtest/gtest.h>

using namespace chess;

namespace {

SearchResult said(std::optional<int> cp, std::optional<int> mate, int depth, const char* best = "") {
    SearchResult result;
    result.scoreCp  = cp;
    result.mateIn   = mate;
    result.depth    = depth;
    result.bestMove = best;
    return result;
}

constexpr int kDeep = GameReview::kMarkDepth + 4;

// A search that has passed the judging depth, saying `cp` there (and `second`
// for its second line), and something else — `later` — by now.
SearchResult noted(int cp, const char* best, std::optional<int> second, int later) {
    SearchResult result = said(later, std::nullopt, GameReview::kJudgeDepth + 3);
    result.pvMove       = "a2a3";  // where it has wandered since
    result.secondCp     = second;
    result.secondDepth  = second ? GameReview::kJudgeDepth + 2 : 0;
    result.noted        = SearchResult::Noted{cp, std::nullopt, best, std::nullopt};
    if (second)
        result.noted->second = SearchResult::Noted::Second{second, std::nullopt};
    return result;
}

Game played(std::initializer_list<const char*> moves, const Position& start = Position::initial()) {
    Game game(start);
    for (const char* move : moves)
        EXPECT_TRUE(game.playUci(move)) << move;
    return game;
}

Position fen(const char* text) {
    const auto position = Position::fromFen(text);
    EXPECT_TRUE(position.has_value()) << text;
    return position.value_or(Position::initial());
}

}  // namespace

TEST(GameReview, KeysAreThePositionCommandsThatLeadToEachPosition) {
    GameReview review;
    const Game game = played({"e2e4", "e7e5"});
    review.follow(game);

    ASSERT_EQ(review.keys().size(), 3u);
    EXPECT_EQ(review.keys()[0], "position startpos");
    EXPECT_EQ(review.keys()[1], "position startpos moves e2e4");
    EXPECT_EQ(review.keys()[2], "position startpos moves e2e4 e7e5");
    EXPECT_EQ(review.now(), game.uciPosition());
}

TEST(GameReview, AGameFromASetPositionIsKeyedByItsFen) {
    const char* start = "4k3/8/8/8/8/8/4P3/4K3 w - - 0 1";
    GameReview  review;
    const Game  game = played({"e2e4"}, fen(start));
    review.follow(game);

    EXPECT_EQ(review.keys()[0], std::string("position fen ") + start);
    EXPECT_EQ(review.now(), game.uciPosition());
}

TEST(GameReview, NoMarkWithoutBothSidesOfTheMove) {
    GameReview review;
    review.follow(played({"a2a3"}));
    EXPECT_FALSE(review.note(review.keys()[0], said(30, std::nullopt, kDeep, "e2e4"), true));
    ASSERT_EQ(review.marks().size(), 1u);
    EXPECT_EQ(review.mark(0), Mark::None);
}

TEST(GameReview, AMoveThatGivesAwayTheGameIsABlunder) {
    GameReview review;
    review.follow(played({"a2a3"}));
    review.note(review.keys()[0], said(30, std::nullopt, kDeep, "e2e4"), true);
    // After the move, from the side to move there: black is winning big.
    EXPECT_TRUE(review.note(review.keys()[1], said(900, std::nullopt, kDeep, "e7e5"), true));
    EXPECT_EQ(review.mark(0), Mark::Blunder);
}

TEST(GameReview, NoMarkFromAShallowLook) {
    GameReview review;
    review.follow(played({"a2a3"}));
    review.note(review.keys()[0], said(30, std::nullopt, GameReview::kMarkDepth - 1, "e2e4"), true);
    review.note(review.keys()[1], said(900, std::nullopt, kDeep, "e7e5"), true);
    EXPECT_EQ(review.mark(0), Mark::None);
}

TEST(GameReview, WhatIsKnownIsNotReplacedByLess) {
    GameReview review;
    const std::string key = review.now();

    review.note(key, said(10, std::nullopt, 12), false);
    review.note(key, said(20, std::nullopt, 10), false);  // shallower: ignored
    EXPECT_EQ(review.found(key)->score.cp, 10);

    review.note(key, said(30, std::nullopt, 16), false);  // deeper: kept
    EXPECT_EQ(review.found(key)->score.cp, 30);

    review.note(key, said(40, std::nullopt, 8, "d2d4"), true);  // finished: kept
    EXPECT_EQ(review.found(key)->score.cp, 40);
    EXPECT_TRUE(review.found(key)->complete);
    EXPECT_EQ(review.found(key)->best, "d2d4");

    review.note(key, said(50, std::nullopt, 30), false);  // anything partial after it: ignored
    EXPECT_EQ(review.found(key)->score.cp, 40);
}

TEST(GameReview, AResultWithNoScoreIsNoFinding) {
    GameReview review;
    review.note(review.now(), said(std::nullopt, std::nullopt, 20, "e2e4"), true);
    EXPECT_EQ(review.found(review.now()), nullptr);
}

TEST(GameReview, TheBestMoveIsTheMoveOfTheLineWhileTheSearchIsRunning) {
    GameReview   review;
    SearchResult result = said(15, std::nullopt, 10);
    result.pvMove       = "g1f3";
    review.note(review.now(), result, false);
    EXPECT_EQ(review.found(review.now())->best, "g1f3");
}

TEST(GameReview, ASecondChoiceFromTooEarlyAnIterationIsLeftOut) {
    GameReview   review;
    SearchResult result = said(30, std::nullopt, 20, "e2e4");
    result.secondCp     = 25;
    result.secondDepth  = 18;
    review.note(review.now(), result, true);
    ASSERT_TRUE(review.found(review.now())->judged.has_value());
    EXPECT_FALSE(review.found(review.now())->judged->second.has_value());

    result.secondDepth = 19;
    review.clear();
    review.note(review.now(), result, true);
    ASSERT_TRUE(review.found(review.now())->judged->second.has_value());
    EXPECT_EQ(review.found(review.now())->judged->second->cp, 25);
}

// The point of judging at a fixed depth: what the engine goes on to say, or
// whether it was let finish, makes no difference to the mark.
TEST(GameReview, AMoveIsJudgedByWhatWasSaidAtTheJudgingDepth) {
    GameReview review;
    review.follow(played({"e2e4"}));

    // Before the move: +0.30 at the judging depth, +3.00 by the time it was
    // cut short. After it: −0.30 for black at the judging depth.
    review.note(review.keys()[0], noted(30, "e2e4", 10, 300), false);
    const auto* found = review.found(review.keys()[0]);
    ASSERT_TRUE(found->judged.has_value());
    EXPECT_EQ(found->judged->depth, GameReview::kJudgeDepth);
    EXPECT_EQ(found->judged->score.cp, 30);
    EXPECT_EQ(found->judged->best, "e2e4");
    EXPECT_EQ(found->judged->second->cp, 10);
    EXPECT_EQ(found->score.cp, 300);  // its latest word is kept too, for the bar
    EXPECT_FALSE(found->complete);

    // A later search of the same position, saying something else there:
    // the first judgement stands.
    review.note(review.keys()[0], noted(-500, "h2h4", -600, 0), true);
    EXPECT_EQ(review.found(review.keys()[0])->judged->score.cp, 30);
    EXPECT_EQ(review.found(review.keys()[0])->score.cp, 0);
}

TEST(GameReview, TheFirstLineAloneAtTheJudgingDepthIsNotYetAJudgement) {
    GameReview   review;
    SearchResult result = noted(30, "e2e4", std::nullopt, 40);
    result.secondCp     = 10;  // the second line is still an iteration behind
    result.secondDepth  = GameReview::kJudgeDepth - 1;
    review.note(review.now(), result, false);
    EXPECT_FALSE(review.found(review.now())->judged.has_value());

    // With one legal move there is no second line to wait for.
    review.clear();
    review.note(review.now(), noted(30, "e2e4", std::nullopt, 40), false);
    EXPECT_TRUE(review.found(review.now())->judged.has_value());
}

// 1. Nf3 Nf6 2. Nc3 Nc6 and 1. Nc3 Nc6 2. Nf3 Nf6 lead to one position.
TEST(GameReview, APositionReachedByAnotherOrderOfMovesIsKnownAlready) {
    GameReview review;
    review.follow(played({"g1f3", "g8f6", "b1c3", "b8c6"}));
    review.note(review.now(), noted(20, "d2d4", 15, 20), true);

    review.follow(played({"b1c3", "b8c6", "g1f3", "g8f6"}));
    const auto* found = review.found(review.now());
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->judged->best, "d2d4");
    // Of the four positions on the way, the last needs no look.
    const auto next = review.nextToReview();
    ASSERT_TRUE(next.has_value());
    EXPECT_NE(next->position, review.now());
}

// The knights go out and come back: the starting position stands a second
// time, and a third would be a draw — it is not the position it was.
TEST(GameReview, APositionStandingForTheSecondTimeIsLookedAtAfresh) {
    GameReview review;
    review.note(review.now(), noted(20, "e2e4", 15, 20), true);

    review.follow(played({"g1f3", "g8f6", "f3g1", "f6g8"}));
    EXPECT_EQ(review.found(review.now()), nullptr);
    EXPECT_NE(review.found(review.keys()[0]), nullptr);  // its first time is still known
}

TEST(GameReview, AMoveTakenBackAndPlayedAgainIsMarkedAgainAtOnce) {
    GameReview review;
    Game       game = played({"a2a3"});
    review.follow(game);
    review.note(review.keys()[0], said(30, std::nullopt, kDeep, "e2e4"), true);
    review.note(review.keys()[1], said(900, std::nullopt, kDeep, "e7e5"), true);

    game.undo();
    EXPECT_TRUE(review.follow(game));
    EXPECT_TRUE(review.marks().empty());

    game.playUci("a2a3");
    EXPECT_TRUE(review.follow(game));
    EXPECT_EQ(review.mark(0), Mark::Blunder);
}

TEST(GameReview, ANewGameForgetsTheOldOne) {
    GameReview review;
    review.note(review.now(), said(30, std::nullopt, kDeep, "e2e4"), true);
    review.clear();
    EXPECT_EQ(review.found("position startpos"), nullptr);
    EXPECT_EQ(review.keys().size(), 1u);
}

TEST(GameReview, StalematingAWonPositionIsABlunderWithNoLookAfterIt) {
    // White mates with Qc8; Qc7 stalemates instead.
    GameReview review;
    review.follow(played({"c1c7"}, fen("k7/8/1K6/8/8/8/8/2Q5 w - - 0 1")));
    review.note(review.keys()[0], said(std::nullopt, 1, kDeep, "c1c8"), true);
    EXPECT_EQ(review.mark(0), Mark::Blunder);
    // And nothing left to look at: the last position is a draw on the board.
    EXPECT_FALSE(review.nextToReview().has_value());
}

TEST(GameReview, TheReviewGoesBackFromTheMostRecentPosition) {
    GameReview review;
    review.follow(played({"e2e4", "e7e5"}));

    auto next = review.nextToReview();
    ASSERT_TRUE(next.has_value());
    EXPECT_EQ(next->position, review.keys()[2]);
    EXPECT_EQ(next->depth, GameReview::kJudgeDepth);
    EXPECT_EQ(next->noteDepth, GameReview::kJudgeDepth);
    EXPECT_EQ(next->moveTimeMs, GameReview::kReviewMs);

    review.note(review.keys()[2], said(30, std::nullopt, 25, "g1f3"), true);
    // Cut short, but past the judging depth: no second look.
    review.note(review.keys()[1], noted(-30, "e7e5", -40, -30), false);
    next = review.nextToReview();
    ASSERT_TRUE(next.has_value());
    EXPECT_EQ(next->position, review.keys()[0]);

    // Cut short too soon: looked at again.
    review.note(review.keys()[0], said(30, std::nullopt, GameReview::kJudgeDepth - 1, "e2e4"), false);
    next = review.nextToReview();
    ASSERT_TRUE(next.has_value());
    EXPECT_EQ(next->position, review.keys()[0]);

    // Looked at to the end without getting deep enough — a slow machine:
    // another look would be no deeper, so there is none.
    review.note(review.keys()[0], said(30, std::nullopt, GameReview::kMarkDepth - 2, "e2e4"), true);
    EXPECT_FALSE(review.nextToReview().has_value());
}

TEST(GameReview, AfterAResignationTheLastPositionIsStillLookedAt) {
    GameReview review;
    Game       game = played({"e2e4"});
    game.resign();
    review.follow(game);
    const auto next = review.nextToReview();
    ASSERT_TRUE(next.has_value());
    EXPECT_EQ(next->position, review.now());
}
