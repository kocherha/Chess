#include "chess/PositionProbe.h"

#include <gtest/gtest.h>

using namespace chess;

namespace {

constexpr const char* kMorphy = "kbK5/pp6/1P6/8/8/8/8/R7 w - - 0 1";  // mate in two
constexpr const char* kStart  = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

// The engine's part: the question asked, once the wait after the edit is over.
SearchRequest asked(PositionProbe& probe) {
    auto work = probe.toEngine();
    EXPECT_TRUE(work.search.has_value()) << "the probe asked the engine nothing";
    return work.search.value_or(SearchRequest{});
}

SearchResult said(const SearchRequest& request, std::optional<int> mate, std::optional<int> cp = {}) {
    SearchResult result;
    result.tag     = request.tag;
    result.mateIn  = mate;
    result.scoreCp = cp;
    return result;
}

}  // namespace

TEST(PositionProbe, AsksAMomentAfterTheEditNotAtOnce) {
    PositionProbe::Memory memory;
    PositionProbe probe(memory);
    probe.look(kStart, true);
    EXPECT_TRUE(probe.waiting());
    EXPECT_FALSE(probe.toEngine().search.has_value());

    probe.tick(PositionProbe::kDelay * 0.5f);
    EXPECT_FALSE(probe.toEngine().search.has_value());
    probe.tick(PositionProbe::kDelay);
    EXPECT_FALSE(probe.waiting());

    const SearchRequest request = asked(probe);
    EXPECT_EQ(request.position, std::string("position fen ") + kStart);
    EXPECT_EQ(request.mate, PositionProbe::kLongestMate);
    EXPECT_EQ(request.moveTimeMs, PositionProbe::kFirstMs);
    EXPECT_LT(request.tag, 0);  // a PuzzleSession's are positive
}

TEST(PositionProbe, NoMateIsAVerdictAndIsRemembered) {
    PositionProbe::Memory memory;
    PositionProbe probe(memory);
    probe.look(kStart, true);
    probe.tick(1.0f);
    EXPECT_FALSE(probe.verdict().has_value());
    probe.takeChanged();

    probe.answer(said(asked(probe), std::nullopt, 35));
    EXPECT_TRUE(probe.takeChanged());
    EXPECT_FALSE(probe.takeChanged());  // once
    ASSERT_TRUE(probe.verdict().has_value());
    EXPECT_EQ(*probe.verdict(), (PositionProbe::Verdict{std::nullopt, 35}));
    EXPECT_EQ(probe.mateToSolve(), 0);
    EXPECT_FALSE(probe.toEngine().search.has_value());  // it has said all it will
    EXPECT_EQ(memory.count(kStart), 1u);
}

TEST(PositionProbe, LooksForAShorterMateUntilThereIsNone) {
    PositionProbe::Memory memory;
    PositionProbe probe(memory);
    probe.look(kMorphy, true);
    probe.tick(1.0f);

    // The first mate it sees is in four.
    probe.answer(said(asked(probe), 4));
    EXPECT_EQ(probe.mateToSolve(), 4);  // shown already, while it goes on looking
    EXPECT_EQ(memory.count(kMorphy), 0u);  // not its last word yet

    SearchRequest shorter = asked(probe);
    EXPECT_EQ(shorter.mate, 3);
    EXPECT_EQ(shorter.moveTimeMs, PositionProbe::kShorterMs);
    probe.answer(said(shorter, 2));  // found one, shorter still
    EXPECT_EQ(probe.mateToSolve(), 2);

    shorter = asked(probe);
    EXPECT_EQ(shorter.mate, 1);
    probe.answer(said(shorter, std::nullopt, 900));  // no mate in one
    EXPECT_EQ(probe.mateToSolve(), 2);
    EXPECT_FALSE(probe.toEngine().search.has_value());
    EXPECT_EQ(memory.at(kMorphy).mate, std::optional<int>(2));
}

TEST(PositionProbe, BeingMatedIsNotAMateToSolve) {
    PositionProbe::Memory memory;
    PositionProbe probe(memory);
    probe.look(kStart, true);
    probe.tick(1.0f);
    probe.answer(said(asked(probe), -3));
    EXPECT_EQ(probe.verdict()->mate, std::optional<int>(-3));
    EXPECT_EQ(probe.mateToSolve(), 0);
    EXPECT_FALSE(probe.toEngine().search.has_value());  // nothing shorter to look for
}

TEST(PositionProbe, AnEditWithdrawsTheQuestionAndItsAnswer) {
    PositionProbe::Memory memory;
    PositionProbe probe(memory);
    probe.look(kStart, true);
    probe.tick(1.0f);
    const SearchRequest old = asked(probe);

    probe.look(kMorphy, true);
    EXPECT_TRUE(probe.toEngine().cancel);
    probe.answer(said(old, std::nullopt, 35));  // about the position before
    EXPECT_FALSE(probe.verdict().has_value());
    EXPECT_TRUE(memory.empty());
}

TEST(PositionProbe, APositionSeenBeforeIsNotAskedAboutAgain) {
    PositionProbe::Memory memory;
    {
        PositionProbe probe(memory);
        probe.look(kMorphy, true);
        probe.tick(1.0f);
        probe.answer(said(asked(probe), 1));
    }
    // Another probe — the editor opened again — with the same memory.
    PositionProbe probe(memory);
    probe.look(kMorphy, true);
    EXPECT_FALSE(probe.waiting());
    EXPECT_EQ(probe.mateToSolve(), 1);
    EXPECT_TRUE(probe.takeChanged());
    probe.tick(1.0f);
    EXPECT_FALSE(probe.toEngine().search.has_value());
}

TEST(PositionProbe, NothingIsAskedAboutAPositionNotWorthAsking) {
    PositionProbe::Memory memory;
    PositionProbe probe(memory);
    probe.look(kStart, false);  // impossible, or over, or no engine
    EXPECT_FALSE(probe.waiting());
    probe.tick(1.0f);
    EXPECT_FALSE(probe.toEngine().search.has_value());

    probe.look(kStart, true);
    probe.drop();  // the editor is left
    probe.tick(1.0f);
    EXPECT_FALSE(probe.toEngine().search.has_value());
}
