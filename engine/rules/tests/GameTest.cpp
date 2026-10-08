#include "chess/Game.h"

#include <gtest/gtest.h>

using namespace chess;

namespace {

Game from(const char* fen) {
    const auto position = Position::fromFen(fen);
    EXPECT_TRUE(position.has_value()) << fen;
    return Game(position.value_or(Position{}));
}

}  // namespace

TEST(Game, ScholarsMate) {
    Game game;
    for (const char* san : {"e4", "e5", "Qh5", "Nc6", "Bc4", "Nf6", "Qxf7#"})
        ASSERT_TRUE(game.playSan(san)) << san;

    const Status status = game.status();
    EXPECT_EQ(status.outcome, Outcome::Checkmate);
    ASSERT_TRUE(status.winner.has_value());
    EXPECT_EQ(*status.winner, Color::White);
    EXPECT_EQ(game.history().back().san, "Qxf7#");

    // Nothing more can be played once it is over.
    EXPECT_FALSE(game.playUci("e8f7"));
}

TEST(Game, IllegalMovesAreRefused) {
    Game game;
    EXPECT_FALSE(game.playUci("e2e5"));
    EXPECT_FALSE(game.playUci("e7e5"));
    EXPECT_FALSE(game.playUci("nonsense"));
    EXPECT_EQ(game.plies(), 0u);
}

// Morphy against the Duke of Brunswick and Count Isouard, Paris 1858: every
// kind of move but en passant and promotion, ending in a mate that is easy to
// check by eye.
TEST(Game, TheOperaGame) {
    Game game;
    for (const char* san : {"e4",   "e5",   "Nf3",  "d6",   "d4",    "Bg4",  "dxe5", "Bxf3", "Qxf3",
                            "dxe5", "Bc4",  "Nf6",  "Qb3",  "Qe7",   "Nc3",  "c6",   "Bg5",  "b5",
                            "Nxb5", "cxb5", "Bxb5+", "Nbd7", "O-O-O", "Rd8",  "Rxd7", "Rxd7", "Rd1",
                            "Qe6",  "Bxd7+", "Nxd7", "Qb8+", "Nxb8",  "Rd8#"})
        ASSERT_TRUE(game.playSan(san)) << san;

    EXPECT_EQ(game.position().fen(), "1n1Rkb1r/p4ppp/4q3/4p1B1/4P3/8/PPP2PPP/2K5 b k - 1 17");
    EXPECT_EQ(game.status().outcome, Outcome::Checkmate);
    EXPECT_EQ(*game.status().winner, Color::White);

    // What was written down is what was typed in: checks, captures and all.
    EXPECT_EQ(game.history()[20].san, "Bxb5+");
    EXPECT_EQ(game.history()[21].san, "Nbd7");
    EXPECT_EQ(game.history()[22].san, "O-O-O");

    // Six men each came off the board; white's included the queen.
    const auto white = game.capturedPieces(Color::White);
    const auto black = game.capturedPieces(Color::Black);
    EXPECT_EQ(white.size(), 6u);
    EXPECT_EQ(white.front(), PieceType::Queen);
    EXPECT_EQ(black.size(), 6u);
    EXPECT_EQ(black.front(), PieceType::Rook);
}

TEST(Game, Stalemate) {
    Game game = from("7k/5Q2/8/8/8/8/8/K7 w - - 0 1");
    ASSERT_TRUE(game.playSan("Qg6"));
    const Status status = game.status();
    EXPECT_EQ(status.outcome, Outcome::Stalemate);
    EXPECT_TRUE(status.drawn());
}

TEST(Game, ThreefoldRepetition) {
    Game game;
    const char* shuffle[] = {"Nf3", "Nf6", "Ng1", "Ng8"};
    for (const char* san : shuffle)
        ASSERT_TRUE(game.playSan(san));
    EXPECT_FALSE(game.status().over());  // twice so far
    for (int i = 0; i < 3; ++i)
        ASSERT_TRUE(game.playSan(shuffle[i]));
    EXPECT_FALSE(game.status().over());
    ASSERT_TRUE(game.playSan("Ng8"));    // the starting position, third time
    EXPECT_EQ(game.status().outcome, Outcome::Repetition);
    EXPECT_TRUE(game.status().drawn());

    // Taking the move back takes the draw back with it.
    ASSERT_TRUE(game.undo());
    EXPECT_FALSE(game.status().over());
}

TEST(Game, LosingCastlingRightsMakesItADifferentPosition) {
    // The kings step aside and come back, twice. The pieces now stand where
    // they started for the third time — but the first of those three still had
    // the right to castle, so by the rule it was a different position.
    Game game = from("4k3/8/8/8/8/8/8/R3K3 w Q - 0 1");
    for (const char* uci : {"e1d1", "e8d8", "d1e1", "d8e8", "e1d1", "e8d8", "d1e1", "d8e8"})
        ASSERT_TRUE(game.playUci(uci)) << uci;
    EXPECT_EQ(game.position().fen(), "4k3/8/8/8/8/8/8/R3K3 w - - 8 5");
    EXPECT_FALSE(game.status().over());

    // One more step and a position *without* the right has occurred three times.
    ASSERT_TRUE(game.playUci("e1d1"));
    EXPECT_EQ(game.status().outcome, Outcome::Repetition);
}

TEST(Game, FiftyMoveRule) {
    Game game = from("4k3/8/8/8/8/8/8/R3K3 w - - 99 80");
    EXPECT_FALSE(game.status().over());
    ASSERT_TRUE(game.playSan("Ra2"));
    EXPECT_EQ(game.status().outcome, Outcome::FiftyMoves);

    // A capture or a pawn move would have reset the count instead.
    Game reset = from("4k3/8/8/8/8/8/r7/R3K3 w - - 99 80");
    ASSERT_TRUE(reset.playSan("Rxa2"));
    EXPECT_NE(reset.status().outcome, Outcome::FiftyMoves);
}

TEST(Game, InsufficientMaterialAfterTheLastCapture) {
    Game game = from("4k3/8/8/8/8/8/4p3/4K3 w - - 0 1");
    EXPECT_FALSE(game.status().over());
    ASSERT_TRUE(game.playSan("Kxe2"));
    EXPECT_EQ(game.status().outcome, Outcome::InsufficientMaterial);
}

TEST(Game, UndoRestoresEverything) {
    Game game;
    const std::string start = game.position().fen();
    ASSERT_TRUE(game.playSan("e4"));
    ASSERT_TRUE(game.playSan("d5"));
    ASSERT_TRUE(game.playSan("exd5"));
    EXPECT_EQ(game.capturedPieces(Color::Black).size(), 1u);
    EXPECT_EQ(game.materialBalance(), 1);

    ASSERT_TRUE(game.undo());
    EXPECT_EQ(game.plies(), 2u);
    EXPECT_TRUE(game.capturedPieces(Color::Black).empty());
    EXPECT_EQ(game.materialBalance(), 0);

    ASSERT_TRUE(game.undo());
    ASSERT_TRUE(game.undo());
    EXPECT_EQ(game.position().fen(), start);
    EXPECT_FALSE(game.undo());
    EXPECT_EQ(game.lastMove(), nullptr);
}

TEST(Game, Resignation) {
    Game game;
    ASSERT_TRUE(game.playSan("e4"));
    game.resign();  // black, to move, gives up
    EXPECT_EQ(game.status().outcome, Outcome::Resignation);
    EXPECT_EQ(*game.status().winner, Color::White);
    EXPECT_FALSE(game.playSan("e5"));

    ASSERT_TRUE(game.undo());  // changes their mind
    EXPECT_FALSE(game.status().over());
    EXPECT_EQ(game.plies(), 1u);
    EXPECT_TRUE(game.playSan("e5"));
}

TEST(Game, EitherSideMayResignWhoeverIsToMove) {
    Game game;
    ASSERT_TRUE(game.playSan("e4"));
    game.resign(Color::White);  // while black is thinking
    EXPECT_EQ(game.status().outcome, Outcome::Resignation);
    EXPECT_EQ(*game.status().winner, Color::Black);
}

TEST(Game, SpeaksUci) {
    Game game;
    EXPECT_EQ(game.uciPosition(), "position startpos");
    ASSERT_TRUE(game.playSan("e4"));
    ASSERT_TRUE(game.playSan("c5"));
    EXPECT_EQ(game.uciPosition(), "position startpos moves e2e4 c7c5");

    Game promoted = from("8/4P1k1/8/8/8/8/8/4K3 w - - 0 1");
    ASSERT_TRUE(promoted.playUci("e7e8q"));
    EXPECT_EQ(promoted.uciPosition(), "position fen 8/4P1k1/8/8/8/8/8/4K3 w - - 0 1 moves e7e8q");
}
