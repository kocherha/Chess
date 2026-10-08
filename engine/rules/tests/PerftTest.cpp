// Perft: count every position reachable in N half-moves and compare with the
// published numbers. It is the standard proof that a move generator is right —
// one missed en passant, one castle through check, one wrong pin, and the
// count is off. The positions below are the well-known set chosen to hit
// exactly those corners.
#include "chess/Position.h"

#include <gtest/gtest.h>

#include <cstdint>

using namespace chess;

namespace {

struct Counts {
    std::uint64_t nodes = 0, captures = 0, enPassant = 0, castles = 0, promotions = 0, checks = 0,
                  mates = 0;
};

void perft(const Position& position, int depth, Counts& counts) {
    for (const Move& move : position.legalMoves()) {
        const Position next = position.after(move);
        if (depth > 1) {
            perft(next, depth - 1, counts);
            continue;
        }
        ++counts.nodes;
        counts.captures   += move.isCapture();
        counts.enPassant  += move.enPassant;
        counts.castles    += move.isCastle();
        counts.promotions += move.promotion != PieceType::None;
        if (next.inCheck()) {
            ++counts.checks;
            counts.mates += !next.hasLegalMoves();
        }
    }
}

Counts perft(const char* fen, int depth) {
    const auto position = Position::fromFen(fen);
    EXPECT_TRUE(position.has_value()) << fen;
    Counts counts;
    if (position)
        perft(*position, depth, counts);
    return counts;
}

constexpr const char* kStart    = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
constexpr const char* kKiwipete = "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1";
constexpr const char* kEndgame  = "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1";
constexpr const char* kPromos   = "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1";
constexpr const char* kTricky   = "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8";
constexpr const char* kMiddle   = "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10";

}  // namespace

TEST(Perft, StartPosition) {
    EXPECT_EQ(perft(kStart, 1).nodes, 20u);
    EXPECT_EQ(perft(kStart, 2).nodes, 400u);

    const Counts three = perft(kStart, 3);
    EXPECT_EQ(three.nodes, 8902u);
    EXPECT_EQ(three.captures, 34u);
    EXPECT_EQ(three.checks, 12u);

    const Counts four = perft(kStart, 4);
    EXPECT_EQ(four.nodes, 197281u);
    EXPECT_EQ(four.captures, 1576u);
    EXPECT_EQ(four.checks, 469u);
    EXPECT_EQ(four.mates, 8u);
}

TEST(Perft, Kiwipete) {
    const Counts one = perft(kKiwipete, 1);
    EXPECT_EQ(one.nodes, 48u);
    EXPECT_EQ(one.captures, 8u);
    EXPECT_EQ(one.castles, 2u);

    const Counts two = perft(kKiwipete, 2);
    EXPECT_EQ(two.nodes, 2039u);
    EXPECT_EQ(two.captures, 351u);
    EXPECT_EQ(two.enPassant, 1u);
    EXPECT_EQ(two.castles, 91u);
    EXPECT_EQ(two.checks, 3u);

    const Counts three = perft(kKiwipete, 3);
    EXPECT_EQ(three.nodes, 97862u);
    EXPECT_EQ(three.captures, 17102u);
    EXPECT_EQ(three.enPassant, 45u);
    EXPECT_EQ(three.castles, 3162u);
    EXPECT_EQ(three.checks, 993u);
    EXPECT_EQ(three.mates, 1u);
}

// Rook endgame full of pins along the fifth rank, including the en passant
// capture that would expose the king to a rook two pawns away.
TEST(Perft, PinnedEnPassant) {
    EXPECT_EQ(perft(kEndgame, 1).nodes, 14u);
    EXPECT_EQ(perft(kEndgame, 2).nodes, 191u);
    EXPECT_EQ(perft(kEndgame, 3).nodes, 2812u);
    EXPECT_EQ(perft(kEndgame, 4).nodes, 43238u);
}

TEST(Perft, Promotions) {
    EXPECT_EQ(perft(kPromos, 1).nodes, 6u);
    EXPECT_EQ(perft(kPromos, 2).nodes, 264u);
    EXPECT_EQ(perft(kPromos, 3).nodes, 9467u);
    EXPECT_EQ(perft(kPromos, 4).nodes, 422333u);
}

TEST(Perft, PromotionWithDiscoveredCheck) {
    EXPECT_EQ(perft(kTricky, 1).nodes, 44u);
    EXPECT_EQ(perft(kTricky, 2).nodes, 1486u);
    EXPECT_EQ(perft(kTricky, 3).nodes, 62379u);
}

TEST(Perft, Middlegame) {
    EXPECT_EQ(perft(kMiddle, 1).nodes, 46u);
    EXPECT_EQ(perft(kMiddle, 2).nodes, 2079u);
    EXPECT_EQ(perft(kMiddle, 3).nodes, 89890u);
}
