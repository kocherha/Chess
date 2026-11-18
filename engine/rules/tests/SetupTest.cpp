#include "chess/Setup.h"

#include <gtest/gtest.h>

#include <algorithm>

using namespace chess;

namespace {

chess::Setup setup(const char* fen) {
    const auto parsed = chess::Setup::fromFen(fen);
    EXPECT_TRUE(parsed.has_value()) << fen;
    return parsed.value_or(chess::Setup{});
}

std::vector<Flaw> flaws(const char* fen) {
    std::vector<Flaw> out;
    for (const Issue& issue : setup(fen).issues())
        out.push_back(issue.flaw);
    return out;
}

bool has(const std::vector<Flaw>& list, Flaw flaw) {
    return std::find(list.begin(), list.end(), flaw) != list.end();
}

const Issue* find(const std::vector<Issue>& issues, Flaw flaw) {
    for (const Issue& issue : issues)
        if (issue.flaw == flaw)
            return &issue;
    return nullptr;
}

Square sq(const char* name) {
    return parseSquare(name);
}

}  // namespace

TEST(Setup, InitialIsLegal) {
    const chess::Setup start = chess::Setup::initial();
    EXPECT_TRUE(start.issues().empty());
    EXPECT_EQ(start.fen(), std::string(kStartFen));
    ASSERT_TRUE(start.position().has_value());
    EXPECT_EQ(*start.position(), Position::initial());
}

TEST(Setup, ReadsWhatPositionWouldRefuse) {
    // Three kings and a pawn on the last rank: not a position, but a setup.
    const chess::Setup odd = setup("KkK4P/8/8/8/8/8/8/8 w - -");
    EXPECT_EQ(odd.count({PieceType::King, Color::White}), 2);
    EXPECT_FALSE(odd.position().has_value());
    EXPECT_FALSE(chess::Setup::fromFen("8/8/8").has_value());
    EXPECT_FALSE(chess::Setup::fromFen("9/8/8/8/8/8/8/8 w - -").has_value());
    // Only the placement is required.
    EXPECT_TRUE(chess::Setup::fromFen("4k3/8/8/8/8/8/8/4K3").has_value());
}

TEST(Setup, MissingKingsAreUnfinishedNotWrong) {
    const auto list = flaws("8/8/8/8/8/8/8/8 w - -");
    ASSERT_EQ(list.size(), 2u);
    EXPECT_EQ(list[0], Flaw::MissingKing);
    EXPECT_EQ(list[1], Flaw::MissingKing);
    EXPECT_EQ(flaws("4k3/8/8/8/8/8/8/8 w - -"), std::vector<Flaw>{Flaw::MissingKing});
}

TEST(Setup, ExtraKing) {
    const auto issues = setup("4k3/8/8/8/8/8/8/K3K3 w - -").issues();
    const Issue* extra = find(issues, Flaw::ExtraKing);
    ASSERT_NE(extra, nullptr);
    EXPECT_EQ(extra->color, Color::White);
    EXPECT_EQ(extra->squares, (std::vector<Square>{sq("a1"), sq("e1")}));
}

TEST(Setup, PawnsOnTheEdge) {
    const auto issues = setup("P3k3/8/8/8/8/8/8/p3K3 w - -").issues();
    ASSERT_EQ(issues.size(), 2u);
    EXPECT_EQ(issues[0].flaw, Flaw::PawnOnEdge);
    EXPECT_EQ(issues[0].squares, std::vector<Square>{sq("a8")});
    EXPECT_EQ(issues[1].squares, std::vector<Square>{sq("a1")});
}

TEST(Setup, KingsSideBySide) {
    const auto list = flaws("8/8/8/3kK3/8/8/8/8 w - -");
    EXPECT_TRUE(has(list, Flaw::KingsTouching));
    // Touching kings attack each other; that is one flaw, not three.
    EXPECT_FALSE(has(list, Flaw::WaitingSideInCheck));
    EXPECT_FALSE(has(list, Flaw::ImpossibleCheck));
    EXPECT_TRUE(flaws("8/8/8/3k1K2/8/8/8/8 w - -").empty());
}

TEST(Setup, SideThatJustMovedIsInCheck) {
    // Black is in check from the rook, and it is white to move.
    const auto issues = setup("4k3/8/8/8/8/8/8/R3K2R w - -").issues();
    EXPECT_TRUE(issues.empty());
    const auto wrong = setup("4k3/8/8/8/8/8/8/4R1K1 w - -").issues();
    const Issue* issue = find(wrong, Flaw::WaitingSideInCheck);
    ASSERT_NE(issue, nullptr);
    EXPECT_EQ(issue->color, Color::Black);
    EXPECT_EQ(issue->squares, (std::vector<Square>{sq("e8"), sq("e1")}));
    // The same with black to move is an ordinary check.
    EXPECT_TRUE(flaws("4k3/8/8/8/8/8/8/4R1K1 b - -").empty());
}

TEST(Setup, Checks) {
    // Double check by rook and knight: the knight moved, uncovering the rook.
    EXPECT_TRUE(flaws("4k3/8/3N4/8/8/8/8/4R1K1 b - -").empty());
    // Two knights cannot both have just moved.
    EXPECT_TRUE(has(flaws("4k3/2N5/3N4/8/8/8/8/6K1 b - -"), Flaw::ImpossibleCheck));
    // Nor a pawn and a knight.
    EXPECT_TRUE(has(flaws("4k3/3P4/5N2/8/8/8/8/6K1 b - -"), Flaw::ImpossibleCheck));
    // Three checks at once.
    EXPECT_TRUE(has(flaws("4k3/3P4/8/7B/8/8/8/4R1K1 b - -"), Flaw::TooManyCheckers));
    // A pawn that has never moved cannot have just given check.
    EXPECT_TRUE(has(flaws("8/8/8/8/8/4k3/3P4/6K1 b - -"), Flaw::ImpossibleCheck));
    EXPECT_TRUE(flaws("8/8/8/8/4k3/3P4/8/6K1 b - -").empty());
}

TEST(Setup, TooManyPawns) {
    const auto issues = setup("4k3/8/8/8/P7/8/PPPPPPPP/4K3 w - -").issues();
    const Issue* issue = find(issues, Flaw::TooManyPawns);
    ASSERT_NE(issue, nullptr);
    EXPECT_EQ(issue->need, 9);
    EXPECT_EQ(issue->squares.size(), 9u);
}

TEST(Setup, PromotedPiecesNeedMissingPawns) {
    // Nine queens and no pawns: eight promotions, eight pawns gone. Fine.
    EXPECT_FALSE(has(flaws("QQQQ1QQQ/QQ6/8/8/8/8/8/4K2k w - -"), Flaw::TooManyPieces));
    // Two queens and all eight pawns: impossible.
    const auto issues = setup("4k3/8/8/8/8/8/PPPPPPPP/QQ2K3 w - -").issues();
    const Issue* issue = find(issues, Flaw::TooManyPieces);
    ASSERT_NE(issue, nullptr);
    EXPECT_EQ(issue->need, 1);
    EXPECT_EQ(issue->have, 0);
    // Three knights with seven pawns: one was promoted.
    EXPECT_FALSE(has(flaws("4k3/8/8/8/8/8/1PPPPPPP/NNN1K3 w - -"), Flaw::TooManyPieces));
}

TEST(Setup, BishopsBySquareColour) {
    // Two light-squared bishops and eight pawns: one of them was promoted.
    EXPECT_TRUE(has(flaws("4k3/8/8/8/8/2B1B3/PPPPPPPP/4K3 w - -"), Flaw::TooManyPieces));
    // One of each, out in the open: the usual pair.
    EXPECT_FALSE(has(flaws("4k3/8/8/8/3PP3/3BB3/PPP2PPP/4K3 w - -"), Flaw::TooManyPieces));
}

TEST(Setup, BishopShutInByItsPawns) {
    // b2 and d2 have not moved, so the c1 bishop never left c1. A dark bishop
    // anywhere else came from a promotion — and with eight pawns there was none.
    EXPECT_TRUE(has(flaws("4k3/8/8/8/8/2B5/PPPPPPPP/4K3 w - -"), Flaw::TooManyPieces));
    EXPECT_FALSE(has(flaws("4k3/8/8/8/8/8/PPPPPPPP/2B1K3 w - -"), Flaw::TooManyPieces));
    // With d2 gone it could have got out.
    EXPECT_FALSE(has(flaws("4k3/8/8/8/3P4/2B5/PPP1PPPP/4K3 w - -"), Flaw::TooManyPieces));
}

TEST(Setup, DoubledPawnsNeedCaptures) {
    // Three white pawns on the a-file took at least three files' worth of
    // captures (b to a, c to b to a), and black has lost nothing.
    const auto issues = setup("rnbqkbnr/pppppppp/8/8/P7/P7/P7/4K3 w - -").issues();
    const Issue* issue = find(issues, Flaw::PawnCaptures);
    ASSERT_NE(issue, nullptr);
    EXPECT_EQ(issue->color, Color::White);
    EXPECT_EQ(issue->need, 3);
    EXPECT_EQ(issue->have, 0);
    // With three black pieces off the board it is possible.
    EXPECT_FALSE(has(flaws("r1bqkb2/pppppppp/8/8/P7/P7/P7/4K3 w - -"), Flaw::PawnCaptures));
    // A pawn on its second rank has not moved and so has not captured.
    EXPECT_TRUE(has(flaws("4k3/8/8/8/8/P7/P7/4K3 w - -"), Flaw::PawnCaptures) == false);
}

TEST(Setup, PawnsThatCannotHaveGotThere) {
    // a2 and b2 never moved, so the pawn on a3 came from b — taken — or a.
    const auto issues = setup("4k3/8/8/8/8/P7/PP6/4K3 w - -").issues();
    const Issue* issue = find(issues, Flaw::PawnCaptures);
    ASSERT_NE(issue, nullptr);
    EXPECT_EQ(issue->need, -1);
}

TEST(Setup, BlackPawnsCountToo) {
    const auto issues = setup("4k3/p7/p7/p7/8/8/PPPPPPPP/RNBQKBNR b - -").issues();
    const Issue* issue = find(issues, Flaw::PawnCaptures);
    ASSERT_NE(issue, nullptr);
    EXPECT_EQ(issue->color, Color::Black);
    EXPECT_EQ(issue->have, 0);
}

TEST(Setup, Castling) {
    chess::Setup s = setup("r3k2r/8/8/8/8/8/8/R3K2R w KQkq -");
    EXPECT_TRUE(s.canCastle(Color::White, Wing::King));
    EXPECT_EQ(s.fen(), "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1");

    // Moving the rook away suspends the right; putting it back restores it.
    s.remove(sq("h1"));
    EXPECT_FALSE(s.canCastle(Color::White, Wing::King));
    EXPECT_TRUE(s.castlingWanted(Color::White, Wing::King));
    EXPECT_EQ(s.fen(), "r3k2r/8/8/8/8/8/8/R3K3 w Qkq - 0 1");
    s.put(sq("h1"), {PieceType::Rook, Color::White});
    EXPECT_TRUE(s.canCastle(Color::White, Wing::King));

    s.setCastling(Color::Black, Wing::Queen, false);
    EXPECT_EQ(s.fen(), "r3k2r/8/8/8/8/8/8/R3K2R w KQk - 0 1");
}

TEST(Setup, EnPassant) {
    chess::Setup s = setup("4k3/8/8/3pP3/8/8/8/4K3 w - -");
    EXPECT_EQ(s.enPassantCandidates(), std::vector<Square>{sq("d6")});
    s.setEnPassant(sq("d6"));
    EXPECT_EQ(s.enPassant(), sq("d6"));
    EXPECT_EQ(s.fen(), "4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1");
    ASSERT_TRUE(s.position().has_value());
    EXPECT_TRUE(s.position()->parseUci("e5d6").has_value());

    // No white pawn beside it: nothing could take, so nothing to offer.
    EXPECT_TRUE(setup("4k3/8/8/3p4/8/8/8/4K3 w - -").enPassantCandidates().empty());
    // The pawn cannot have come from d7 if something stands there.
    EXPECT_TRUE(setup("4k3/3p4/8/3pP3/8/8/8/4K3 w - -").enPassantCandidates().empty());

    // Black to move switches whose pawn it is, and forgets the square.
    s.setSideToMove(Color::Black);
    EXPECT_EQ(s.enPassant(), kNoSquare);
}

TEST(Setup, EnPassantMustExplainTheCheck) {
    // Black has just played d7-d5. White's king on e4 is in check from that
    // pawn: consistent.
    chess::Setup s = setup("7k/8/8/3pP3/4K3/8/8/8 w - d6");
    EXPECT_EQ(s.enPassant(), sq("d6"));
    EXPECT_TRUE(s.issues().empty());
    // In check from a rook along the first rank, which d7-d5 could not
    // have uncovered: impossible.
    EXPECT_TRUE(has(flaws("7k/8/8/3pP3/8/8/8/r3K3 w - d6"), Flaw::BadEnPassant));
    // Without the en passant square the same position is fine.
    EXPECT_TRUE(flaws("7k/8/8/3pP3/8/8/8/r3K3 w - -").empty());
    // Uncovered by the pawn leaving d7: the bishop on c8 sees g4 through it.
    EXPECT_TRUE(flaws("2b4k/8/8/3pP3/6K1/8/8/8 w - d6").empty());
}

TEST(Setup, EveryLegalTestPositionPasses) {
    // Positions from games and test suites; none of them may be flagged.
    // (The fourth of the usual perft positions is not among them: white's
    // pawns there needed five captures and black has lost one piece. It is a
    // test of move generation, not a position from a game.)
    const char* fens[] = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
        "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
        "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",
        "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10",
    };
    for (const char* fen : fens) {
        const chess::Setup s = setup(fen);
        EXPECT_TRUE(s.issues().empty()) << fen << ": flaw " << static_cast<int>(s.issues().front().flaw);
        ASSERT_TRUE(s.position().has_value()) << fen;
        EXPECT_EQ(s.position()->fen(), Position::fromFen(fen)->fen());
    }
}

TEST(Setup, RoundTripsThroughPosition) {
    const Position position = *Position::fromFen("r3k2r/8/8/3pP3/8/8/8/R3K2R w Kq d6 0 12");
    const chess::Setup s = chess::Setup::from(position);
    EXPECT_EQ(s.fen(), position.fen());
    EXPECT_EQ(*s.position(), position);
}

TEST(Setup, NoPositionFromAGameIsFlagged) {
    // Every position of a few hundred random games — which promote, capture
    // and give odd checks far more often than real ones — must pass. A flaw
    // found here would be a position the rules have just produced.
    std::uint32_t seed = 12345;
    const auto next = [&seed](std::size_t bound) {
        seed = seed * 1664525u + 1013904223u;
        return static_cast<std::size_t>((seed >> 8) % bound);
    };
    int positions = 0;
    for (int game = 0; game < 300; ++game) {
        Position position = Position::initial();
        for (int ply = 0; ply < 300; ++ply) {
            const auto moves = position.legalMoves();
            if (moves.empty())
                break;
            position.apply(moves[next(moves.size())]);
            const chess::Setup s = chess::Setup::from(position);
            const auto issues    = s.issues();
            ++positions;
            ASSERT_TRUE(issues.empty()) << position.fen() << ": flaw " << static_cast<int>(issues.front().flaw);
            // The same position, except that an en passant square nobody
            // could use is not written down.
            const auto again = Position::fromFen(s.fen());
            ASSERT_TRUE(again.has_value()) << s.fen();
            ASSERT_EQ(again->repetitionKey(), position.repetitionKey());
        }
    }
    EXPECT_GT(positions, 30000);
}
