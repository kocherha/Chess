#include "chess/BoardMarks.h"

#include <gtest/gtest.h>

using namespace chess;

namespace {

Game play(std::initializer_list<const char*> moves) {
    Game game;
    for (const char* move : moves)
        EXPECT_TRUE(game.playUci(move)) << move;
    return game;
}

constexpr Square kE1 = 4, kE8 = 60;

}  // namespace

TEST(BoardMarks, TheLastMoveAndNothingElseInAQuietPosition) {
    const BoardMarks marks = marksFor(play({"e2e4"}), {});
    EXPECT_EQ(marks.lastFrom, parseSquare("e2"));
    EXPECT_EQ(marks.lastTo, parseSquare("e4"));
    EXPECT_EQ(marks.check, kNoSquare);
    EXPECT_EQ(marks.kings[0].sign, GameSign::None);
    EXPECT_EQ(marks.kings[1].sign, GameSign::None);
    EXPECT_EQ(marks.verdict, Mark::None);
}

TEST(BoardMarks, AKingInCheckIsLitAndSigned) {
    const BoardMarks marks = marksFor(play({"e2e4", "f7f6", "d1h5"}), {});
    EXPECT_EQ(marks.check, kE8);
    EXPECT_EQ(marks.kings[1].square, kE8);  // black's, in black's place
    EXPECT_EQ(marks.kings[1].sign, GameSign::Check);
    EXPECT_EQ(marks.kings[0].sign, GameSign::None);
}

TEST(BoardMarks, MateIsOneAndHash) {
    const BoardMarks marks = marksFor(play({"f2f3", "e7e5", "g2g4", "d8h4"}), {});
    EXPECT_EQ(marks.kings[0], (KingSign{kE1, GameSign::Mate}));
    EXPECT_EQ(marks.kings[1], (KingSign{kE8, GameSign::Won}));
}

TEST(BoardMarks, ResignationIsOneAndNought) {
    Game game = play({"e2e4"});
    game.resign(Color::Black);
    const BoardMarks marks = marksFor(game, {});
    EXPECT_EQ(marks.kings[0].sign, GameSign::Won);
    EXPECT_EQ(marks.kings[1].sign, GameSign::Resigned);
}

TEST(BoardMarks, ADrawIsAHalfForBoth) {
    const auto stalemate = Position::fromFen("7k/5Q2/6K1/8/8/8/8/8 b - - 0 1");
    ASSERT_TRUE(stalemate.has_value());
    const BoardMarks marks = marksFor(Game(*stalemate), {});
    EXPECT_EQ(marks.kings[0].sign, GameSign::Draw);
    EXPECT_EQ(marks.kings[1].sign, GameSign::Draw);
}

TEST(BoardMarks, TheLastMovesMarkOnlyOnceEveryMoveHasOne) {
    const Game game = play({"e2e4", "e7e5"});
    EXPECT_EQ(marksFor(game, {Mark::Great}).verdict, Mark::None);  // the last move's is not in yet
    EXPECT_EQ(marksFor(game, {Mark::None, Mark::Blunder}).verdict, Mark::Blunder);
}
