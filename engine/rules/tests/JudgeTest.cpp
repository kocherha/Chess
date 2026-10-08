// Marks are a rule applied to an engine's numbers, so these tests supply the
// numbers by hand: no engine is involved, and what is checked is the rule.
#include "chess/Game.h"
#include "chess/Judge.h"

#include <gtest/gtest.h>

using namespace chess;

namespace {

Position at(const char* fen) {
    const auto position = Position::fromFen(fen);
    EXPECT_TRUE(position.has_value()) << fen;
    return position.value_or(Position{});
}

Move uci(const Position& position, const char* text) {
    const auto move = position.parseUci(text);
    EXPECT_TRUE(move.has_value()) << text;
    return move.value_or(Move{});
}

Score cp(int value) {
    Score score;
    score.cp = value;
    return score;
}

Score mate(int moves) {
    Score score;
    score.mate = moves;
    return score;
}

// The position after 1.e4 e5 2.Nf3 d6 3.Bc4 Bg4 4.Nc3 g6, where 5.Nxe5 leaves
// the queen to be taken and wins: Légal's mate.
Position legal() {
    Game game;
    for (const char* san : {"e4", "e5", "Nf3", "d6", "Bc4", "Bg4", "Nc3", "g6"})
        EXPECT_TRUE(game.playSan(san)) << san;
    return game.position();
}

}  // namespace

// --- the curve ---------------------------------------------------------------------

TEST(Judge, WinChanceIsTheCurveTheBarUses) {
    EXPECT_DOUBLE_EQ(winChance(0), 0.5);
    EXPECT_NEAR(winChance(100), 0.591, 0.001);
    EXPECT_NEAR(winChance(300), 0.751, 0.001);
    EXPECT_NEAR(winChance(500), 0.863, 0.001);
    EXPECT_NEAR(winChance(-300), 1.0 - winChance(300), 1e-12);

    EXPECT_DOUBLE_EQ(winChance(mate(3)), 1.0);
    EXPECT_DOUBLE_EQ(winChance(mate(-3)), 0.0);
    EXPECT_DOUBLE_EQ(winChance(mate(0)), 0.0);  // the side to move has been mated
    EXPECT_DOUBLE_EQ(winChance(Score{}), 0.5);  // nothing known
}

TEST(Judge, Suffixes) {
    EXPECT_STREQ(suffix(Mark::Brilliant), "!!");
    EXPECT_STREQ(suffix(Mark::Great), "!");
    EXPECT_STREQ(suffix(Mark::Inaccuracy), "?!");
    EXPECT_STREQ(suffix(Mark::Mistake), "?");
    EXPECT_STREQ(suffix(Mark::Blunder), "??");
    EXPECT_STREQ(suffix(Mark::None), "");
}

// --- exchanges ---------------------------------------------------------------------

TEST(Exchange, CountsTheWholeSequence) {
    // Bishop takes a pawn that a pawn defends: a pawn won, a bishop lost.
    Position guarded = at("4k3/8/3p4/4p3/8/2B5/8/4K3 w - - 0 1");
    EXPECT_EQ(guarded.exchange(uci(guarded, "c3e5")), -2);

    // Rook takes a knight nothing defends.
    Position loose = at("4k3/8/8/4n3/8/8/8/4RK2 w - - 0 1");
    EXPECT_EQ(loose.exchange(uci(loose, "e1e5")), 3);

    // Not a capture at all.
    EXPECT_EQ(loose.exchange(uci(loose, "e1e2")), 0);
}

TEST(Exchange, APinnedDefenderDoesNotDefend) {
    // The knight on a6 guards c5 — unless the rook on the a-file pins it to
    // its king.
    Position pinned = at("k7/8/n7/2p5/8/4B3/8/R3K3 w - - 0 1");
    EXPECT_EQ(pinned.exchange(uci(pinned, "e3c5")), 1);

    Position free = at("k7/8/n7/2p5/8/4B3/8/4K2R w - - 0 1");
    EXPECT_EQ(free.exchange(uci(free, "e3c5")), -2);
}

TEST(Exchange, AKingDoesNotTakeWhatIsProtected) {
    // The queen takes on f7 with the bishop behind her: the king is the only
    // defender and may not take back.
    Position position = at("4k3/5p2/8/8/2B5/8/8/4KQ2 w - - 0 1");
    EXPECT_EQ(position.exchange(uci(position, "f1f7")), 1);
}

// --- what a move gave up ---------------------------------------------------------

TEST(Judge, ThresholdsOfABadMove) {
    const Position start = Position::initial();
    const Move played    = uci(start, "e2e4");

    Evidence evidence;
    evidence.best     = cp(0);  // level before...
    evidence.bestMove = "d2d4"; // ...and something else was best

    // ...and afterwards the opponent stands this much better.
    const auto after = [&](int opponent) {
        evidence.after = cp(opponent);
        return judge(start, played, evidence);
    };
    EXPECT_EQ(after(0), Mark::None);
    EXPECT_EQ(after(40), Mark::None);         // 3.7% of the winning chance
    EXPECT_EQ(after(60), Mark::Inaccuracy);   // 5.5%
    EXPECT_EQ(after(100), Mark::Inaccuracy);  // 9.1%
    EXPECT_EQ(after(115), Mark::Mistake);     // 10.4%
    EXPECT_EQ(after(180), Mark::Blunder);     // 16%
    EXPECT_EQ(after(900), Mark::Blunder);

    evidence.after = mate(2);  // walked into a mate
    EXPECT_EQ(judge(start, played, evidence), Mark::Blunder);
}

TEST(Judge, TheSameLossMattersLessWhenTheGameIsDecided) {
    const Position start = Position::initial();
    const Move played    = uci(start, "e2e4");

    // A pawn and a half thrown away in a level game is a mistake...
    Evidence level{cp(0), "d2d4", std::nullopt, cp(150)};
    EXPECT_EQ(judge(start, played, level), Mark::Mistake);

    // ...and nothing at all when nine pawns up before and seven and a half after.
    Evidence won{cp(900), "d2d4", std::nullopt, cp(-750)};
    EXPECT_EQ(judge(start, played, won), Mark::None);
}

TEST(Judge, TheEnginesOwnChoiceIsNeverMarkedDown) {
    const Position start = Position::initial();
    const Move played    = uci(start, "e2e4");

    // The two searches disagree wildly; the move was still the one recommended.
    Evidence evidence{cp(30), "e2e4", std::nullopt, cp(500)};
    EXPECT_DOUBLE_EQ(loss(played, evidence), 0.0);
    EXPECT_EQ(judge(start, played, evidence), Mark::None);
}

TEST(Judge, MissingAMateIsALoss) {
    const Position start = Position::initial();
    const Move played    = uci(start, "e2e4");

    Evidence stillWinning{mate(2), "d2d4", std::nullopt, cp(-1200)};
    EXPECT_EQ(judge(start, played, stillWinning), Mark::None);  // 99% is still 99%

    Evidence thrown{mate(2), "d2d4", std::nullopt, cp(-100)};
    EXPECT_EQ(judge(start, played, thrown), Mark::Blunder);
}

TEST(Judge, NothingIsSaidWithoutBothScores) {
    const Position start = Position::initial();
    const Move played    = uci(start, "e2e4");
    EXPECT_EQ(judge(start, played, Evidence{Score{}, "d2d4", std::nullopt, cp(900)}), Mark::None);
    EXPECT_EQ(judge(start, played, Evidence{cp(0), "d2d4", std::nullopt, Score{}}), Mark::None);
}

// --- good moves --------------------------------------------------------------------

TEST(Judge, TheOnlyMoveThatHoldsIsGreat) {
    const Position start = Position::initial();
    const Move played    = uci(start, "e2e4");

    Evidence alone{cp(30), "e2e4", cp(-200), cp(-30)};
    EXPECT_EQ(judge(start, played, alone), Mark::Great);

    // With a second move nearly as good there was nothing to find.
    Evidence company{cp(30), "e2e4", cp(-20), cp(-30)};
    EXPECT_EQ(judge(start, played, company), Mark::None);

    // Nor is there when the engine was not asked for a second move.
    Evidence unknown{cp(30), "e2e4", std::nullopt, cp(-30)};
    EXPECT_EQ(judge(start, played, unknown), Mark::None);

    // Being the only way to lose slowly earns nothing.
    Evidence lost{cp(-400), "e2e4", mate(-2), cp(400)};
    EXPECT_EQ(judge(start, played, lost), Mark::None);
}

TEST(Judge, TheObviousCaptureIsNotGreat) {
    // A knight to be had for nothing: far and away the best move, and no find.
    const Position loose = at("4k3/8/8/4n3/8/8/8/4RK2 w - - 0 1");
    Evidence evidence{cp(600), "e1e5", cp(0), cp(-600)};
    EXPECT_EQ(judge(loose, uci(loose, "e1e5"), evidence), Mark::None);
}

TEST(Judge, TakingBackIsNotGreat) {
    // Black has just taken a knight on f3. Taking the bishop back is the only
    // move that does not leave white a piece down — and an even trade, since
    // the queen then takes the knight.
    const Position position = at("5qk1/8/8/8/8/5b2/3N4/K7 w - - 0 1");
    const Move played       = uci(position, "d2f3");
    ASSERT_FALSE(position.inCheck());
    ASSERT_EQ(position.exchange(played), 0);

    Move previous;
    previous.from     = parseSquare("g4");
    previous.to       = parseSquare("f3");
    previous.piece    = PieceType::Bishop;
    previous.captured = PieceType::Knight;

    Evidence evidence{cp(0), "d2f3", cp(-300), cp(0)};
    EXPECT_EQ(judge(position, played, evidence, &previous), Mark::None);
    // The same capture with nothing just taken there would have been a find.
    EXPECT_EQ(judge(position, played, evidence), Mark::Great);
}

TEST(Judge, AnsweringACheckIsNotGreat) {
    // Three ways out of the check, and only one that does not lose the rook.
    const Position position = at("4k3/8/8/8/8/8/4r3/R3K3 w - - 0 1");
    ASSERT_TRUE(position.inCheck());
    Evidence evidence{cp(0), "e1e2", cp(-500), cp(0)};
    EXPECT_EQ(judge(position, uci(position, "e1e2"), evidence), Mark::None);
}

TEST(Judge, SavingWhatWasAboutToBeTakenIsNotGreat) {
    // The queen is attacked by a pawn and has one safe square that keeps
    // everything else in order. Finding it took no finding.
    const Position position = at("4k3/8/8/2p5/3Q4/8/8/4K3 w - - 0 1");
    ASSERT_TRUE(position.passed().has_value());
    Evidence evidence{cp(800), "d4d1", cp(-100), cp(-800)};
    EXPECT_EQ(judge(position, uci(position, "d4d1"), evidence), Mark::None);

    // The same gap with nothing under attack is a find.
    const Position quiet = at("4k3/8/8/8/3Q4/8/8/4K3 w - - 0 1");
    EXPECT_EQ(judge(quiet, uci(quiet, "d4d1"), evidence), Mark::Great);
}

TEST(Position, PassingHandsOverTheMove) {
    const Position start = Position::initial();
    const auto passed    = start.passed();
    ASSERT_TRUE(passed.has_value());
    EXPECT_EQ(passed->sideToMove(), Color::Black);
    EXPECT_EQ(passed->legalMoves().size(), 20u);

    // The chance to take en passant goes with the move.
    const Position chance = at("4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 2");
    ASSERT_TRUE(chance.passed().has_value());
    EXPECT_EQ(chance.passed()->enPassantSquare(), kNoSquare);

    // A side in check cannot pass.
    EXPECT_FALSE(at("4k3/8/8/8/8/8/4r3/R3K3 w - - 0 1").passed().has_value());
}

TEST(Judge, AForcedMoveIsNotAFind) {
    // One legal move: the pawn. Whatever the engine thinks of it, playing it
    // took no thought.
    const Position position = at("8/7p/8/7P/8/1q6/2k5/K7 w - - 0 1");
    ASSERT_EQ(position.legalMoves().size(), 1u);
    Evidence evidence{cp(0), "h5h6", cp(-900), cp(0)};
    EXPECT_EQ(judge(position, uci(position, "h5h6"), evidence), Mark::None);
}

TEST(Judge, ASoundSacrificeIsBrilliant) {
    const Position position = legal();
    const Move played       = uci(position, "f3e5");  // the queen is left hanging

    Evidence evidence{cp(350), "f3e5", cp(60), cp(-350)};
    EXPECT_EQ(judge(position, played, evidence), Mark::Brilliant);

    // Not if it does not work...
    Evidence unsound{cp(60), "d2d3", cp(50), cp(500)};
    EXPECT_EQ(judge(position, played, unsound), Mark::Blunder);

    // ...and not if the game was won without it.
    Evidence won{cp(900), "f3e5", cp(800), cp(-900)};
    EXPECT_EQ(judge(position, played, won), Mark::None);
}

TEST(Judge, ATradeIsNotASacrifice) {
    Game game;
    for (const char* san : {"e4", "e5", "Nf3", "Nc6", "Bb5", "a6"})
        ASSERT_TRUE(game.playSan(san)) << san;
    const Position position = game.position();

    // Bishop takes knight, pawn takes bishop: nothing was given up.
    Evidence evidence{cp(20), "b5c6", cp(15), cp(-20)};
    EXPECT_EQ(judge(position, uci(position, "b5c6"), evidence), Mark::None);
}
