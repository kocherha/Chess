#include "chess/MoveList.h"

#include <gtest/gtest.h>

using namespace chess;

namespace {

std::vector<std::string> moves(int count) {
    std::vector<std::string> out;
    for (int i = 0; i < count; ++i)
        out.push_back("m" + std::to_string(i));
    return out;
}

}  // namespace

TEST(MoveList, ARowIsAMoveOfEachSide) {
    MoveList list;
    list.setMoves(moves(5));
    EXPECT_EQ(list.rowCount(), 3);
    EXPECT_EQ(list.number(0), 1);
    EXPECT_EQ(list.number(2), 3);
    EXPECT_EQ(list.moveIn(0, 0), 0);
    EXPECT_EQ(list.moveIn(0, 1), 1);
    EXPECT_EQ(list.moveIn(2, 0), 4);
    EXPECT_EQ(list.moveIn(2, 1), -1);  // black has not moved yet
}

TEST(MoveList, AGameThatBeganWithBlackToMoveHasAnEmptyFirstCell) {
    MoveList list;
    list.setMoves(moves(3), false, 23);
    EXPECT_EQ(list.rowCount(), 2);
    EXPECT_EQ(list.moveIn(0, 0), -1);
    EXPECT_EQ(list.moveIn(0, 1), 0);
    EXPECT_EQ(list.moveIn(1, 0), 1);
    EXPECT_EQ(list.moveIn(1, 1), 2);
    EXPECT_EQ(list.number(0), 23);
    EXPECT_EQ(list.number(1), 24);
}

TEST(MoveList, NumberingStartsAtOneAtTheLeast) {
    MoveList list;
    list.setMoves(moves(2), true, 0);
    EXPECT_EQ(list.number(0), 1);
}

TEST(MoveList, TheResultHasARowOfItsOwn) {
    MoveList list;
    EXPECT_TRUE(list.empty());
    list.setResult("1–0");
    EXPECT_EQ(list.rowCount(), 1);  // a game resigned before any move
    EXPECT_FALSE(list.empty());

    list.setMoves(moves(4));
    EXPECT_EQ(list.rowCount(), 3);
    EXPECT_TRUE(list.isResultRow(2));
    EXPECT_FALSE(list.isResultRow(1));
    EXPECT_EQ(list.moveIn(2, 0), -1);
}

TEST(MoveList, MarksMayBeFewerThanMoves) {
    MoveList list;
    list.setMoves(moves(3));
    EXPECT_TRUE(list.setMarks({Mark::Blunder}));
    EXPECT_FALSE(list.setMarks({Mark::Blunder}));
    EXPECT_EQ(list.mark(0), Mark::Blunder);
    EXPECT_EQ(list.mark(2), Mark::None);
    EXPECT_EQ(list.mark(-1), Mark::None);
}

TEST(MoveList, TheListFollowsItsEnd) {
    MoveList list;
    list.setVisibleRows(3);
    list.setMoves(moves(20));  // 10 rows
    EXPECT_EQ(list.firstRow(), 7);
    EXPECT_EQ(list.lastFirstRow(), 7);
    list.setMoves(moves(21));
    EXPECT_EQ(list.firstRow(), 8);
}

TEST(MoveList, ScrolledAwayItStaysThereUntilThePresentChanges) {
    MoveList list;
    list.setVisibleRows(3);
    list.setMoves(moves(20));
    EXPECT_TRUE(list.scroll(2.0f));
    EXPECT_EQ(list.firstRow(), 5);

    list.setMarks({Mark::Inaccuracy});  // marks arrive: the list stays
    EXPECT_EQ(list.firstRow(), 5);
    list.setVisibleRows(4);  // the window grew
    EXPECT_EQ(list.firstRow(), 5);

    list.setMoves(moves(21));  // a move: back to the end
    EXPECT_EQ(list.firstRow(), list.lastFirstRow());
}

TEST(MoveList, TheEndOfTheGameIsShownWhereverTheListWas) {
    MoveList list;
    list.setVisibleRows(3);
    list.setMoves(moves(20));
    list.scroll(5.0f);
    list.setResult("½–½");
    EXPECT_EQ(list.firstRow(), list.lastFirstRow());
    EXPECT_TRUE(list.isResultRow(list.firstRow() + 2));
}

TEST(MoveList, FractionsOfARowAddUp) {
    MoveList list;
    list.setVisibleRows(3);
    list.setMoves(moves(20));
    EXPECT_FALSE(list.scroll(0.4f));
    EXPECT_FALSE(list.scroll(0.4f));
    EXPECT_TRUE(list.scroll(0.4f));
    EXPECT_EQ(list.firstRow(), 6);
}

TEST(MoveList, NothingIsOwedPastAnEnd) {
    MoveList list;
    list.setVisibleRows(3);
    list.setMoves(moves(20));
    EXPECT_FALSE(list.scroll(-1.5f));  // already at the end
    // Had the half row been kept, this would have been a whole one.
    EXPECT_FALSE(list.scroll(0.6f));
    EXPECT_EQ(list.firstRow(), 7);
}

TEST(MoveList, TheThumbTakesTheListToAShareOfTheWay) {
    MoveList list;
    list.setVisibleRows(3);
    list.setMoves(moves(20));  // the first row goes from 0 to 7
    EXPECT_TRUE(list.scrollTo(0.0f));
    EXPECT_EQ(list.firstRow(), 0);
    EXPECT_TRUE(list.scrollTo(0.5f));
    EXPECT_EQ(list.firstRow(), 4);
    EXPECT_FALSE(list.scrollTo(0.52f));
    list.setMarks({Mark::Mistake});
    EXPECT_EQ(list.firstRow(), 4);  // not following
    EXPECT_TRUE(list.scrollTo(1.5f));
    EXPECT_EQ(list.firstRow(), 7);
    list.setVisibleRows(2);  // following again: stays at the end
    EXPECT_EQ(list.firstRow(), 8);
}

TEST(MoveList, AClickIsAPressAndAReleaseOnTheSameMove) {
    MoveList list;
    list.setMoves(moves(4));
    list.press(2);
    EXPECT_EQ(list.release(2), 2);
    EXPECT_EQ(list.release(2), std::nullopt);  // only once
    list.press(2);
    EXPECT_EQ(list.release(3), std::nullopt);
    list.press(-1);
    EXPECT_EQ(list.release(-1), std::nullopt);
}

TEST(MoveList, NewMovesForgetWhatThePointerWasOn) {
    MoveList list;
    list.setMoves(moves(4));
    EXPECT_TRUE(list.setHovered(1));
    EXPECT_FALSE(list.setHovered(1));
    EXPECT_TRUE(list.setRinged(2));
    list.press(3);

    list.setMoves(moves(5));
    EXPECT_EQ(list.hovered(), -1);
    EXPECT_EQ(list.ringed(), -1);
    EXPECT_EQ(list.release(3), std::nullopt);
}
