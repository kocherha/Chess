#include "chess/Position.h"

#include <gtest/gtest.h>

#include <algorithm>

using namespace chess;

namespace {

Position at(const char* fen) {
    std::string error;
    const auto position = Position::fromFen(fen, &error);
    EXPECT_TRUE(position.has_value()) << fen << ": " << error;
    return position.value_or(Position{});
}

std::string sanOf(const Position& position, const char* uci) {
    const auto move = position.parseUci(uci);
    EXPECT_TRUE(move.has_value()) << uci << " is not legal in " << position.fen();
    return move ? position.san(*move) : std::string();
}

bool legal(const Position& position, const char* uci) {
    return position.parseUci(uci).has_value();
}

}  // namespace

TEST(Squares, Names) {
    EXPECT_EQ(squareName(0), "a1");
    EXPECT_EQ(squareName(63), "h8");
    EXPECT_EQ(parseSquare("e4"), makeSquare(4, 3));
    EXPECT_EQ(parseSquare("i4"), kNoSquare);
    EXPECT_EQ(parseSquare("e9"), kNoSquare);
    EXPECT_EQ(parseSquare("e"), kNoSquare);
}

TEST(Fen, RoundTrips) {
    for (const char* fen : {
             "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
             "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
             "rnbqkbnr/ppp1p1pp/8/3pPp2/8/8/PPPP1PPP/RNBQKBNR w KQkq f6 0 3",
             "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 b - - 12 47",
         }) {
        EXPECT_EQ(at(fen).fen(), fen);
    }
}

TEST(Fen, ClocksAreOptional) {
    const auto position = Position::fromFen("4k3/8/8/8/8/8/8/4K3 b - -");
    ASSERT_TRUE(position.has_value());
    EXPECT_EQ(position->fen(), "4k3/8/8/8/8/8/8/4K3 b - - 0 1");
}

TEST(Fen, RejectsNonsense) {
    std::string error;
    EXPECT_FALSE(Position::fromFen("", &error));
    EXPECT_FALSE(error.empty());
    EXPECT_FALSE(Position::fromFen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP w KQkq - 0 1"));         // seven ranks
    EXPECT_FALSE(Position::fromFen("rnbqkbnr/ppppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1")); // nine pawns in a rank
    EXPECT_FALSE(Position::fromFen("rnbq1bnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"));  // no black king
    EXPECT_FALSE(Position::fromFen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR x KQkq - 0 1"));  // whose move?
    EXPECT_FALSE(Position::fromFen("P3k3/8/8/8/8/8/8/4K3 w - - 0 1"));                            // pawn on the last rank
    // White to move while black is already in check: black could not have got here.
    EXPECT_FALSE(Position::fromFen("4k3/8/8/8/8/8/8/4RK2 w - - 0 1"));
}

TEST(Fen, DropsCastlingRightsThePlacementContradicts) {
    // Rights claimed for a rook that is not there.
    EXPECT_EQ(at("4k3/8/8/8/8/8/8/R3K3 w KQkq - 0 1").fen(), "4k3/8/8/8/8/8/8/R3K3 w Q - 0 1");
}

TEST(Moves, TwentyFromTheStart) {
    const Position position = Position::initial();
    EXPECT_EQ(position.legalMoves().size(), 20u);
    EXPECT_EQ(position.legalMovesFrom(parseSquare("g1")).size(), 2u);
    EXPECT_EQ(position.legalMovesFrom(parseSquare("e1")).size(), 0u);
    EXPECT_EQ(position.legalMovesFrom(parseSquare("e7")).size(), 0u);  // not black's move
    EXPECT_FALSE(position.inCheck());
}

TEST(Moves, PinnedPieceMayOnlyMoveAlongThePin) {
    // The e-file knight is pinned by the rook; the bishop is pinned on the
    // diagonal and may slide along it or take the pinner.
    const Position position = at("4r1k1/8/8/8/1b6/8/3BN3/4K3 w - - 0 1");
    EXPECT_TRUE(position.legalMovesFrom(parseSquare("e2")).empty());
    const auto bishop = position.legalMovesFrom(parseSquare("d2"));
    ASSERT_EQ(bishop.size(), 2u);
    EXPECT_TRUE(legal(position, "d2c3"));
    EXPECT_TRUE(legal(position, "d2b4"));
}

TEST(Moves, InCheckOnlyAnswersToTheCheckAreLegal) {
    const Position position = at("4k3/8/8/8/8/8/4r3/R3K3 w Q - 0 1");
    EXPECT_TRUE(position.inCheck());
    const auto moves = position.legalMoves();
    // Take the rook, or step aside to d1/f1. Castling out of check is not allowed.
    EXPECT_EQ(moves.size(), 3u);
    EXPECT_FALSE(legal(position, "e1c1"));
}

TEST(Castling, BothSidesWhenTheWayIsClear) {
    const Position position = at("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1");
    EXPECT_EQ(sanOf(position, "e1g1"), "O-O");
    EXPECT_EQ(sanOf(position, "e1c1"), "O-O-O");

    const Position castled = position.after(*position.parseUci("e1g1"));
    EXPECT_EQ(castled.fen(), "r3k2r/8/8/8/8/8/8/R4RK1 b kq - 1 1");
    const Position longCastled = castled.after(*castled.parseUci("e8c8"));
    EXPECT_EQ(longCastled.fen(), "2kr3r/8/8/8/8/8/8/R4RK1 w - - 2 2");
}

TEST(Castling, NotThroughOrIntoCheck) {
    // A black rook on f8 covers f1: the king may not pass it. Queenside is fine.
    const Position through = at("4kr2/8/8/8/8/8/8/R3K2R w KQ - 0 1");
    EXPECT_FALSE(legal(through, "e1g1"));
    EXPECT_TRUE(legal(through, "e1c1"));

    // A rook on g8 covers the landing square.
    const Position into = at("4k1r1/8/8/8/8/8/8/R3K2R w KQ - 0 1");
    EXPECT_FALSE(legal(into, "e1g1"));

    // b1 attacked is no obstacle: the king never crosses it.
    const Position bFile = at("1r2k3/8/8/8/8/8/8/R3K2R w KQ - 0 1");
    EXPECT_TRUE(legal(bFile, "e1c1"));

    // ...but a piece standing on b1 is.
    const Position blocked = at("4k3/8/8/8/8/8/8/RN2K2R w KQ - 0 1");
    EXPECT_FALSE(legal(blocked, "e1c1"));
}

TEST(Castling, RightsAreLostForGood) {
    Position position = at("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1");
    position.apply(*position.parseUci("h1h8"));  // rook takes rook: both h-side rights go
    EXPECT_EQ(position.fen(), "r3k2R/8/8/8/8/8/8/R3K3 b Qq - 0 1");
    position.apply(*position.parseUci("e8e7"));  // the king moves: black has none left
    EXPECT_EQ(position.fen(), "r6R/4k3/8/8/8/8/8/R3K3 w Q - 1 2");
}

TEST(EnPassant, CaptureRemovesThePawnBesideNotTheTarget) {
    Position position = at("rnbqkbnr/ppp1p1pp/8/3pPp2/8/8/PPPP1PPP/RNBQKBNR w KQkq f6 0 3");
    EXPECT_EQ(sanOf(position, "e5f6"), "exf6");
    const auto move = position.parseUci("e5f6");
    ASSERT_TRUE(move.has_value());
    EXPECT_TRUE(move->enPassant);
    EXPECT_EQ(move->captured, PieceType::Pawn);
    position.apply(*move);
    EXPECT_EQ(position.fen(), "rnbqkbnr/ppp1p1pp/5P2/3p4/8/8/PPPP1PPP/RNBQKBNR b KQkq - 0 3");
    // The d-pawn's chance was a move earlier; d6 is not available.
    EXPECT_FALSE(legal(at("rnbqkbnr/ppp1p1pp/8/3pPp2/8/8/PPPP1PPP/RNBQKBNR w KQkq f6 0 3"), "e5d6"));
}

TEST(EnPassant, OnlyOnTheVeryNextMove) {
    Position position = Position::initial();
    for (const char* uci : {"e2e4", "a7a6", "e4e5", "d7d5"})
        position.apply(*position.parseUci(uci));
    EXPECT_EQ(position.enPassantSquare(), parseSquare("d6"));
    EXPECT_TRUE(legal(position, "e5d6"));

    for (const char* uci : {"a2a3", "a6a5"})
        position.apply(*position.parseUci(uci));
    EXPECT_FALSE(legal(position, "e5d6"));
}

TEST(Promotion, FourChoicesAndThePieceMustBeNamed) {
    const Position position = at("8/4P1k1/8/8/8/8/8/4K3 w - - 0 1");
    const Square from = parseSquare("e7"), to = parseSquare("e8");
    EXPECT_TRUE(position.isPromotion(from, to));
    EXPECT_FALSE(position.findMove(from, to).has_value());
    EXPECT_TRUE(position.findMove(from, to, PieceType::Queen).has_value());
    EXPECT_TRUE(position.findMove(from, to, PieceType::Knight).has_value());
    EXPECT_EQ(position.legalMovesFrom(from).size(), 4u);
    EXPECT_FALSE(legal(position, "e7e8"));
    EXPECT_FALSE(legal(position, "e7e8k"));
    EXPECT_EQ(position.after(*position.parseUci("e7e8n")).fen(), "4N3/6k1/8/8/8/8/8/4K3 b - - 0 1");
}

TEST(San, Basics) {
    const Position start = Position::initial();
    EXPECT_EQ(sanOf(start, "e2e4"), "e4");
    EXPECT_EQ(sanOf(start, "g1f3"), "Nf3");

    const Position capture = at("rnbqkbnr/ppp1pppp/8/3p4/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 0 2");
    EXPECT_EQ(sanOf(capture, "e4d5"), "exd5");
}

TEST(San, Disambiguation) {
    // Two knights can reach d2: by file.
    const Position knights = at("4k3/8/8/8/8/5N2/8/1N2K3 w - - 0 1");
    EXPECT_EQ(sanOf(knights, "b1d2"), "Nbd2");
    EXPECT_EQ(sanOf(knights, "f3d2"), "Nfd2");

    // Two rooks on one file: by rank.
    const Position rooks = at("R7/4k3/8/8/8/8/8/R3K3 w - - 0 1");
    EXPECT_EQ(sanOf(rooks, "a1a4"), "R1a4");
    EXPECT_EQ(sanOf(rooks, "a8a4"), "R8a4");

    // Three queens that can all reach e1. The one on h4 shares its rank with
    // one twin and its file with the other, so it needs the whole square.
    const Position queens = at("8/8/1k6/8/4Q2Q/8/8/K6Q w - - 0 1");
    EXPECT_EQ(sanOf(queens, "h4e1"), "Qh4e1");
    EXPECT_EQ(sanOf(queens, "e4e1"), "Qee1");
    EXPECT_EQ(sanOf(queens, "h1e1"), "Q1e1");

    // A pinned twin does not count: it has no legal move there.
    const Position pinned = at("4r1k1/8/8/8/8/5N2/4N3/4K3 w - - 0 1");
    EXPECT_EQ(sanOf(pinned, "f3d4"), "Nd4");
}

TEST(San, ChecksMatesAndPromotions) {
    const Position mate = at("6k1/5ppp/8/8/8/8/8/R3K3 w - - 0 1");
    EXPECT_EQ(sanOf(mate, "a1a8"), "Ra8#");

    const Position promote = at("6k1/4P3/8/8/8/8/8/4K3 w - - 0 1");
    EXPECT_EQ(sanOf(promote, "e7e8q"), "e8=Q+");

    const Position takeAndPromote = at("3r2k1/4P3/8/8/8/8/8/4K3 w - - 0 1");
    EXPECT_EQ(sanOf(takeAndPromote, "e7d8r"), "exd8=R+");
    EXPECT_EQ(sanOf(takeAndPromote, "e7e8n"), "e8=N");
}

TEST(San, ParsesWhatItWrites) {
    const Position position = at("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1");
    for (const Move& move : position.legalMoves()) {
        const auto parsed = position.parseSan(position.san(move));
        ASSERT_TRUE(parsed.has_value()) << position.san(move);
        EXPECT_TRUE(parsed->sameAs(move)) << position.san(move);
    }
    EXPECT_TRUE(position.parseSan("0-0").has_value());
    EXPECT_TRUE(position.parseSan("Nxf7!?").has_value());
    EXPECT_FALSE(position.parseSan("Nf9").has_value());
    EXPECT_FALSE(position.parseSan("").has_value());
}

TEST(Material, WhoCanStillMate) {
    EXPECT_TRUE(at("4k3/8/8/8/8/8/8/4K3 w - - 0 1").insufficientMaterial());
    EXPECT_TRUE(at("4k3/8/8/8/8/8/8/4KN2 w - - 0 1").insufficientMaterial());
    EXPECT_TRUE(at("4k3/8/8/8/8/8/8/4KB2 w - - 0 1").insufficientMaterial());
    // Bishops on the same colour, one each: still nothing.
    EXPECT_TRUE(at("4kb2/8/8/8/8/8/8/2B1K3 w - - 0 1").insufficientMaterial());
    // Opposite colours: a helpmate exists, so the game goes on.
    EXPECT_FALSE(at("5b2/4k3/8/8/8/8/8/2B1KB2 w - - 0 1").insufficientMaterial());
    EXPECT_FALSE(at("4k3/8/8/8/8/8/8/4KNN1 w - - 0 1").insufficientMaterial());
    EXPECT_FALSE(at("4k3/8/8/8/8/8/4P3/4K3 w - - 0 1").insufficientMaterial());
    EXPECT_FALSE(at("4k3/8/8/8/8/8/8/4KR2 w - - 0 1").insufficientMaterial());
}

TEST(Repetition, EnPassantOnlyCountsWhenItCanBeUsed) {
    // After 1.e4 nobody can take on e3, so this is "the same position" as the
    // one without the en passant square.
    const Position pushed = at("rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1");
    const Position plain  = at("rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1");
    EXPECT_EQ(pushed.repetitionKey(), plain.repetitionKey());

    // Here the capture is real, and so is the difference.
    const Position real    = at("rnbqkbnr/ppp1p1pp/8/3pPp2/8/8/PPPP1PPP/RNBQKBNR w KQkq f6 0 3");
    const Position without = at("rnbqkbnr/ppp1p1pp/8/3pPp2/8/8/PPPP1PPP/RNBQKBNR w KQkq - 0 3");
    EXPECT_NE(real.repetitionKey(), without.repetitionKey());
}
