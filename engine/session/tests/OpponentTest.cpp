#include "chess/Opponent.h"

#include "chess/Pause.h"

#include <gtest/gtest.h>

using namespace chess;

namespace {

// Level 8: no random moves, two seconds a move. And a level whose every move
// is random.
Level engineOnly() { return levelByNumber(8); }
Level allRandom() {
    Level level           = levelByNumber(1);
    level.randomMoveShare = 1.0;
    return level;
}

// The computer plays black; the person has just played e4.
Game afterE4() {
    Game game;
    EXPECT_TRUE(game.playUci("e2e4"));
    return game;
}

SearchResult answer(const SearchRequest& request, const char* best) {
    SearchResult result;
    result.tag      = request.tag;
    result.bestMove = best;
    return result;
}

}  // namespace

TEST(Opponent, AsksTheEngineOnItsTurnAndPlaysWhatItSays) {
    Opponent opponent(1);
    opponent.newGame(Color::Black, engineOnly());
    opponent.follow(afterE4(), true);

    auto work = opponent.toEngine();
    ASSERT_TRUE(work.search.has_value());
    EXPECT_EQ(work.search->position, "position startpos moves e2e4");
    EXPECT_EQ(work.search->moveTimeMs, engineOnly().moveTimeMs);
    EXPECT_GT(work.search->tag, 0);
    EXPECT_FALSE(opponent.takeMove().has_value());  // not before the engine has said

    opponent.answer(answer(*work.search, "e7e5"));
    const auto move = opponent.takeMove();
    ASSERT_TRUE(move.has_value());
    EXPECT_EQ(move->uci(), "e7e5");
    EXPECT_FALSE(opponent.takeMove().has_value());  // once
}

TEST(Opponent, AsksNothingOnThePersonsTurnOrOnceTheGameIsOver) {
    Opponent opponent(1);
    opponent.newGame(Color::Black, engineOnly());
    opponent.follow(Game(), true);  // white to move: the person's turn
    EXPECT_FALSE(opponent.toEngine().search.has_value());

    Game mated;
    for (const char* move : {"f2f3", "e7e5", "g2g4", "d8h4"})
        ASSERT_TRUE(mated.playUci(move));
    opponent.newGame(Color::White, engineOnly());
    opponent.follow(mated, true);  // white to move, and mated
    EXPECT_FALSE(opponent.toEngine().search.has_value());
    EXPECT_FALSE(opponent.lost());
}

TEST(Opponent, AnAnswerToAWithdrawnQuestionIsNotPlayed) {
    Opponent opponent(1);
    opponent.newGame(Color::Black, engineOnly());
    opponent.follow(afterE4(), true);
    const SearchRequest asked = *opponent.toEngine().search;

    // The person takes e4 back while the engine thinks.
    opponent.follow(Game(), true);
    EXPECT_TRUE(opponent.toEngine().cancel);

    opponent.answer(answer(asked, "e7e5"));
    EXPECT_FALSE(opponent.takeMove().has_value());
}

TEST(Opponent, TheSameMoveAgainGetsTheSameAnswerAfterAPause) {
    Opponent opponent(1);
    opponent.newGame(Color::Black, engineOnly());
    opponent.follow(afterE4(), true);
    opponent.answer(answer(*opponent.toEngine().search, "c7c5"));
    ASSERT_TRUE(opponent.takeMove().has_value());

    // Taken back, and e4 played again: nothing is asked, the answer comes
    // from memory once the pause is over.
    opponent.follow(Game(), true);
    opponent.follow(afterE4(), true);
    EXPECT_FALSE(opponent.toEngine().search.has_value());
    EXPECT_TRUE(opponent.pausing());

    opponent.tick(kReplyPause * 0.5f);
    EXPECT_FALSE(opponent.takeMove().has_value());
    opponent.tick(kReplyPause);
    const auto move = opponent.takeMove();
    ASSERT_TRUE(move.has_value());
    EXPECT_EQ(move->uci(), "c7c5");
    EXPECT_FALSE(opponent.pausing());
}

TEST(Opponent, ANewGameForgetsTheAnswersOfTheLastOne) {
    Opponent opponent(1);
    opponent.newGame(Color::Black, engineOnly());
    opponent.follow(afterE4(), true);
    opponent.answer(answer(*opponent.toEngine().search, "c7c5"));
    ASSERT_TRUE(opponent.takeMove().has_value());

    opponent.newGame(Color::Black, engineOnly());
    opponent.follow(afterE4(), true);
    EXPECT_TRUE(opponent.toEngine().search.has_value());
}

TEST(Opponent, BelowTheEnginesFloorAMoveIsPickedAtRandomAndRemembered) {
    Opponent opponent(7);
    opponent.newGame(Color::Black, allRandom());
    opponent.follow(afterE4(), true);
    EXPECT_FALSE(opponent.toEngine().search.has_value());  // the engine is not asked
    ASSERT_TRUE(opponent.pausing());

    opponent.tick(kReplyPause + 0.35f);  // the longest such a pause can be
    const auto move = opponent.takeMove();
    ASSERT_TRUE(move.has_value());
    EXPECT_TRUE(afterE4().position().parseUci(move->uci()).has_value());

    // And taking e4 back does not roll again.
    opponent.follow(Game(), true);
    opponent.follow(afterE4(), true);
    opponent.tick(kReplyPause);
    const auto again = opponent.takeMove();
    ASSERT_TRUE(again.has_value());
    EXPECT_EQ(again->uci(), move->uci());
}

TEST(Opponent, AnImpossibleAnswerOrNoEngineMeansItIsLost) {
    Opponent opponent(1);
    opponent.newGame(Color::Black, engineOnly());
    opponent.follow(afterE4(), true);
    opponent.answer(answer(*opponent.toEngine().search, "e2e4"));  // not black's move
    EXPECT_TRUE(opponent.lost());
    EXPECT_NE(opponent.trouble().find("e2e4"), std::string::npos) << opponent.trouble();
    EXPECT_FALSE(opponent.takeMove().has_value());

    Opponent alone(1);
    alone.newGame(Color::Black, engineOnly());
    alone.follow(afterE4(), false);
    EXPECT_TRUE(alone.lost());
    EXPECT_FALSE(alone.toEngine().search.has_value());

    // A new game starts it afresh.
    alone.newGame(Color::Black, engineOnly());
    EXPECT_FALSE(alone.lost());
}

TEST(Opponent, AnEngineThatDiesInTheMiddleOfAGameIsReported) {
    Opponent opponent(1);
    opponent.newGame(Color::Black, engineOnly());
    opponent.follow(afterE4(), true);
    const SearchRequest asked = *opponent.toEngine().search;
    opponent.engineFailed("the engine stopped answering");
    EXPECT_TRUE(opponent.lost());
    EXPECT_EQ(opponent.trouble(), "the engine stopped answering");
    opponent.answer(answer(asked, "e7e5"));  // too late to count
    EXPECT_FALSE(opponent.takeMove().has_value());
}

TEST(Opponent, ARandomMoveIsAlwaysLegalAndPromotesToAQueen) {
    std::mt19937 rng(3);
    // White's only moves: the pawn to b8, as anything, and king moves.
    const auto position = Position::fromFen("7k/1P6/8/8/8/8/8/K7 w - - 0 1");
    ASSERT_TRUE(position.has_value());
    for (int i = 0; i < 50; ++i) {
        const auto move = randomMove(*position, rng);
        ASSERT_TRUE(move.has_value());
        EXPECT_TRUE(move->promotion == PieceType::None || move->promotion == PieceType::Queen);
    }
    EXPECT_FALSE(randomMove(*Position::fromFen("7k/5Q2/6K1/8/8/8/8/8 b - - 0 1"), rng).has_value());  // stalemate
}
