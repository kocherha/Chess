#include "chess/PuzzleSession.h"

#include <gtest/gtest.h>

using namespace chess;
using Phase = PuzzleSession::Phase;
using Event = PuzzleSession::Event;
using Note  = PuzzleSession::Note;

namespace {

// Mate in one: Re8#. And mate in two: Morphy's Ra6 bxa6 b7#.
constexpr const char* kBackRank = "6k1/5ppp/8/8/8/8/5PPP/4R1K1 w - - 0 1";
constexpr const char* kMorphy   = "kbK5/pp6/1P6/8/8/8/8/R7 w - - 0 1";
// A position with no forced mate, for a best-move problem.
constexpr const char* kStart = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

Move uci(const PuzzleSession& session, const char* text) {
    const auto move = session.game().position().parseUci(text);
    EXPECT_TRUE(move.has_value()) << text;
    return move.value_or(Move{});
}

// The engine, played by the test: what it was asked, and an answer to it.
SearchRequest asked(PuzzleSession& session) {
    auto work = session.toEngine();
    EXPECT_TRUE(work.search.has_value()) << "the session asked the engine nothing";
    return work.search.value_or(SearchRequest{});
}

// A question asked before: the session answers it from what the engine said
// then, and the engine is not asked at all.
void answeredFromMemory(PuzzleSession& session) {
    auto work = session.toEngine();
    EXPECT_FALSE(work.search.has_value()) << "the engine was asked again";
    ASSERT_TRUE(work.known.has_value()) << "no answer from memory";
    session.answer(*work.known);
}

SearchResult answerTo(const SearchRequest& request, const char* best, std::optional<int> mate,
                      std::optional<int> cp = std::nullopt) {
    SearchResult result;
    result.tag      = request.tag;
    result.bestMove = best;
    result.mateIn   = mate;
    result.scoreCp  = cp;
    return result;
}

std::vector<Event::Kind> kinds(const std::vector<Event>& events) {
    std::vector<Event::Kind> out;
    for (const Event& event : events)
        out.push_back(event.kind);
    return out;
}

PuzzleSession opened(const char* fen, int mateIn, bool engine = true) {
    PuzzleSession session;
    session.setEngineUsable(engine);
    EXPECT_TRUE(session.open(fen, mateIn));
    session.takeEvents();
    return session;
}

}  // namespace

TEST(PuzzleSession, RefusesAnImpossiblePosition) {
    PuzzleSession session;
    std::string error;
    EXPECT_FALSE(session.open("8/8/8/8/8/8/8/8 w - - 0 1", 1, &error));
    EXPECT_FALSE(error.empty());
    EXPECT_FALSE(session.isOpen());
}

TEST(PuzzleSession, MateInOneByThePlayer) {
    PuzzleSession session = opened(kBackRank, 1);
    EXPECT_EQ(session.phase(), Phase::Ready);
    EXPECT_TRUE(session.playerToMove());
    EXPECT_FALSE(session.toEngine().search.has_value());  // nothing to ask before the move

    ASSERT_TRUE(session.play(uci(session, "e1e8")));
    EXPECT_EQ(session.phase(), Phase::Solved);
    EXPECT_EQ(session.note().kind, Note::Kind::SolvedAlone);
    const auto events = session.takeEvents();
    ASSERT_EQ(kinds(events), (std::vector{Event::Kind::Moved, Event::Kind::Finished}));
    EXPECT_EQ(events[0].move, "e1e8");
    EXPECT_FALSE(events[0].bySolution);
    EXPECT_TRUE(events[1].byPlayer);
    EXPECT_FALSE(events[1].helped);
    EXPECT_FALSE(session.playerToMove());
}

TEST(PuzzleSession, AMoveThatIsNotTheMateWhenNoneAreLeft) {
    PuzzleSession session = opened(kBackRank, 1);
    ASSERT_TRUE(session.play(uci(session, "g1h1")));
    // Mate in one, and it was not mate: wrong without asking anyone.
    EXPECT_EQ(session.phase(), Phase::Wrong);
    EXPECT_EQ(session.note().kind, Note::Kind::NoMate);
    EXPECT_EQ(session.note().move, "Kh1");
    EXPECT_EQ(session.note().movesLeft, 1);
    EXPECT_EQ(session.marks().back(), Mark::Mistake);
    EXPECT_FALSE(session.toEngine().search.has_value());

    // It stays on the board for a moment, then goes by itself.
    session.takeEvents();
    session.tick(PuzzleSession::kWrongSeconds * 0.5f);
    EXPECT_EQ(session.game().plies(), 1u);
    session.tick(PuzzleSession::kWrongSeconds);
    EXPECT_EQ(session.game().plies(), 0u);
    EXPECT_EQ(session.phase(), Phase::Ready);
    const auto events = session.takeEvents();
    ASSERT_EQ(kinds(events), (std::vector{Event::Kind::TakenBack}));
    EXPECT_TRUE(events[0].automatic);
    EXPECT_EQ(session.note().kind, Note::Kind::NoMate);  // still says what was wrong
}

TEST(PuzzleSession, MateInTwoWithTheDefence) {
    PuzzleSession session = opened(kMorphy, 2);
    ASSERT_TRUE(session.play(uci(session, "a1a6")));
    EXPECT_EQ(session.phase(), Phase::Checking);

    // Asked: from the position before, by this move only, a mate in two?
    const SearchRequest check = asked(session);
    EXPECT_EQ(check.mate, 2);
    EXPECT_EQ(check.searchMoves, std::vector<std::string>{"a1a6"});
    EXPECT_EQ(check.position, "position fen " + std::string(kMorphy));

    session.answer(answerTo(check, "a1a6", 2));
    EXPECT_EQ(session.phase(), Phase::Replying);
    EXPECT_EQ(session.note().kind, Note::Kind::Right);
    EXPECT_EQ(session.marks().back(), Mark::Great);  // the key move

    const SearchRequest defence = asked(session);
    EXPECT_EQ(defence.mate, 0);
    session.answer(answerTo(defence, "b7a6", -1));
    EXPECT_EQ(session.phase(), Phase::Replying);
    EXPECT_TRUE(session.timerRunning());
    session.takeEvents();

    session.tick(PuzzleSession::kReplyPause + 0.01f);
    EXPECT_EQ(session.phase(), Phase::Ready);
    const auto events = session.takeEvents();
    ASSERT_EQ(kinds(events), (std::vector{Event::Kind::Replied}));
    EXPECT_EQ(events[0].move, "b7a6");
    EXPECT_EQ(session.movesLeft(), 1);

    ASSERT_TRUE(session.play(uci(session, "b6b7")));
    EXPECT_EQ(session.phase(), Phase::Solved);
    EXPECT_EQ(session.note().kind, Note::Kind::SolvedAlone);
}

TEST(PuzzleSession, WhatTheAttackDoesNotSeeTheDefenceIsAskedAbout) {
    PuzzleSession session = opened(kMorphy, 2);
    ASSERT_TRUE(session.play(uci(session, "a1a6")));
    const SearchRequest check = asked(session);
    session.answer(answerTo(check, "a1a6", std::nullopt, 900));  // no mate found that way
    EXPECT_EQ(session.phase(), Phase::Checking);

    const SearchRequest recheck = asked(session);
    EXPECT_EQ(recheck.position, session.game().uciPosition());  // after the move
    session.answer(answerTo(recheck, "b7a6", -1));                // mated in one: right after all
    EXPECT_EQ(session.phase(), Phase::Replying);
    EXPECT_EQ(session.note().kind, Note::Kind::Right);
}

TEST(PuzzleSession, AWrongMoveInAMateProblem) {
    PuzzleSession session = opened(kMorphy, 2);
    ASSERT_TRUE(session.play(uci(session, "a1a2")));
    session.answer(answerTo(asked(session), "a1a2", std::nullopt, 300));
    session.answer(answerTo(asked(session), "a8a7", std::nullopt, -300));
    EXPECT_EQ(session.phase(), Phase::Wrong);
    EXPECT_EQ(session.note().kind, Note::Kind::NoMate);
    EXPECT_EQ(session.note().movesLeft, 2);
    EXPECT_FALSE(session.toEngine().search.has_value());  // nothing more to ask
}

TEST(PuzzleSession, TheSameWrongMoveTriedAgainIsJudgedFromMemory) {
    PuzzleSession session = opened(kMorphy, 2);
    ASSERT_TRUE(session.play(uci(session, "a1a2")));
    session.answer(answerTo(asked(session), "a1a2", std::nullopt, 300));
    session.answer(answerTo(asked(session), "a8a7", std::nullopt, -300));
    ASSERT_EQ(session.phase(), Phase::Wrong);
    session.tick(PuzzleSession::kWrongSeconds + 0.1f);  // taken back
    ASSERT_EQ(session.phase(), Phase::Ready);

    ASSERT_TRUE(session.play(uci(session, "a1a2")));
    answeredFromMemory(session);  // is it still mate in two? no...
    answeredFromMemory(session);  // ...and the defence is not mated either
    EXPECT_EQ(session.phase(), Phase::Wrong);
    EXPECT_EQ(session.note().kind, Note::Kind::NoMate);
}

TEST(PuzzleSession, TheSolutionAfterAHintStartsFromWhatTheHintFound) {
    PuzzleSession session = opened(kMorphy, 2);
    session.hint();
    session.answer(answerTo(asked(session), "a1a6", 2));
    session.takeEvents();

    session.showSolution();
    answeredFromMemory(session);  // the same question as the hint's
    const auto events = session.takeEvents();
    ASSERT_FALSE(events.empty());
    EXPECT_EQ(events.back().kind, Event::Kind::Moved);
    EXPECT_EQ(events.back().move, "a1a6");
}

TEST(PuzzleSession, AnswersToOldQuestionsAreIgnored) {
    PuzzleSession session = opened(kMorphy, 2);
    ASSERT_TRUE(session.play(uci(session, "a1a6")));
    const SearchRequest check = asked(session);

    session.takeBack();  // the question is no longer asked
    EXPECT_TRUE(session.toEngine().cancel);
    EXPECT_EQ(session.phase(), Phase::Ready);
    EXPECT_EQ(session.game().plies(), 0u);

    session.answer(answerTo(check, "a1a6", 2));
    EXPECT_EQ(session.phase(), Phase::Ready);
    EXPECT_FALSE(session.toEngine().search.has_value());
}

TEST(PuzzleSession, TakingBackGoesToTheSolversPreviousTurn) {
    PuzzleSession session = opened(kMorphy, 2);
    ASSERT_TRUE(session.play(uci(session, "a1a6")));
    session.answer(answerTo(asked(session), "a1a6", 2));
    session.answer(answerTo(asked(session), "b7a6", -1));
    session.tick(1.0f);
    ASSERT_EQ(session.game().plies(), 2u);
    session.takeEvents();

    session.takeBack();
    EXPECT_EQ(session.game().plies(), 0u);  // the reply and the move before it
    EXPECT_TRUE(session.marks().empty());
    EXPECT_EQ(session.note().kind, Note::Kind::None);
    const auto events = session.takeEvents();
    ASSERT_EQ(kinds(events), (std::vector{Event::Kind::TakenBack}));
    EXPECT_FALSE(events[0].automatic);
}

TEST(PuzzleSession, AHintInAMateProblem) {
    PuzzleSession session = opened(kMorphy, 2);
    session.hint();
    EXPECT_TRUE(session.helped());
    EXPECT_EQ(session.note().kind, Note::Kind::LookingForHint);
    const SearchRequest request = asked(session);
    EXPECT_EQ(request.mate, 2);

    session.answer(answerTo(request, "a1a6", 2));
    EXPECT_EQ(session.note().kind, Note::Kind::Hint);
    EXPECT_EQ(session.note().move, "Ra6");
    EXPECT_EQ(session.hintFrom(), parseSquare("a1"));
    EXPECT_EQ(session.hintTo(), parseSquare("a6"));

    // Solved after a hint: solved, but helped.
    ASSERT_TRUE(session.play(uci(session, "a1a6")));
    EXPECT_EQ(session.hintFrom(), kNoSquare);
    session.answer(answerTo(asked(session), "a1a6", 2));
    session.answer(answerTo(asked(session), "b7a6", -1));
    session.tick(1.0f);
    session.takeEvents();
    ASSERT_TRUE(session.play(uci(session, "b6b7")));
    EXPECT_EQ(session.note().kind, Note::Kind::SolvedHelped);
    const auto events = session.takeEvents();
    ASSERT_FALSE(events.empty());
    EXPECT_TRUE(events.back().helped);
}

TEST(PuzzleSession, TheSolutionOfAMateProblem) {
    PuzzleSession session = opened(kMorphy, 2);
    session.showSolution();
    EXPECT_TRUE(session.showingSolution());
    EXPECT_FALSE(session.playerToMove());

    session.answer(answerTo(asked(session), "a1a6", 2));  // the hint is played, not shown
    auto events = session.takeEvents();
    ASSERT_EQ(kinds(events), (std::vector{Event::Kind::Moved}));
    EXPECT_TRUE(events[0].bySolution);
    EXPECT_EQ(session.phase(), Phase::Replying);  // the engine's own move is not checked

    session.answer(answerTo(asked(session), "b7a6", -1));
    session.tick(1.0f);
    session.answer(answerTo(asked(session), "b6b7", 1));  // and the next one is asked for
    events = session.takeEvents();
    ASSERT_EQ(kinds(events), (std::vector{Event::Kind::Replied, Event::Kind::Moved, Event::Kind::Finished}));
    EXPECT_FALSE(events.back().byPlayer);
    EXPECT_EQ(session.note().kind, Note::Kind::SolutionShown);
    EXPECT_FALSE(session.showingSolution());
}

TEST(PuzzleSession, ABestMoveProblem) {
    PuzzleSession session = opened(kStart, 0);
    EXPECT_EQ(session.phase(), Phase::Preparing);
    EXPECT_FALSE(session.playerToMove());
    const SearchRequest prepare = asked(session);
    EXPECT_EQ(prepare.depth, PuzzleSession::kBestDepth);
    session.answer(answerTo(prepare, "e2e4", std::nullopt, 30));
    EXPECT_EQ(session.phase(), Phase::Ready);

    // Near enough to the best is right.
    ASSERT_TRUE(session.play(uci(session, "d2d4")));
    const SearchRequest check = asked(session);
    EXPECT_EQ(check.searchMoves, std::vector<std::string>{"d2d4"});
    session.answer(answerTo(check, "d2d4", std::nullopt, 25));
    EXPECT_EQ(session.phase(), Phase::Solved);
    EXPECT_EQ(session.note().kind, Note::Kind::BestRight);
    EXPECT_EQ(session.note().best, "e4");
    EXPECT_EQ(session.marks().back(), Mark::Great);
}

TEST(PuzzleSession, AClearlyWorseMoveInABestMoveProblem) {
    PuzzleSession session = opened(kStart, 0);
    session.answer(answerTo(asked(session), "e2e4", std::nullopt, 30));
    ASSERT_TRUE(session.play(uci(session, "g2g4")));
    session.answer(answerTo(asked(session), "g2g4", std::nullopt, -120));
    EXPECT_EQ(session.phase(), Phase::Wrong);
    EXPECT_EQ(session.note().kind, Note::Kind::BestWrong);
    EXPECT_EQ(session.note().move, "g4");
    EXPECT_EQ(session.note().best, "e4");
}

// The bug of 2026-11-18: after solving (or failing) a best-move problem,
// "show the solution" started it again and never played the move.
TEST(PuzzleSession, TheSolutionOfABestMoveProblemAfterItWasSolved) {
    PuzzleSession session = opened(kStart, 0);
    session.answer(answerTo(asked(session), "e2e4", std::nullopt, 30));
    ASSERT_TRUE(session.play(uci(session, "e2e4")));
    session.answer(answerTo(asked(session), "e2e4", std::nullopt, 30));
    ASSERT_EQ(session.phase(), Phase::Solved);
    session.takeEvents();

    session.showSolution();
    EXPECT_EQ(session.phase(), Phase::Preparing);  // from the start
    EXPECT_EQ(session.game().plies(), 0u);
    EXPECT_TRUE(session.showingSolution());
    answeredFromMemory(session);  // the best move is known already

    const auto events = session.takeEvents();
    ASSERT_EQ(kinds(events), (std::vector{Event::Kind::Opened, Event::Kind::Moved, Event::Kind::Finished}));
    EXPECT_TRUE(events[1].bySolution);
    EXPECT_FALSE(events[2].byPlayer);
    EXPECT_TRUE(events[2].helped);
    EXPECT_EQ(session.phase(), Phase::Solved);
}

TEST(PuzzleSession, TheSolutionOfABestMoveProblemAfterAWrongMove) {
    PuzzleSession session = opened(kStart, 0);
    session.answer(answerTo(asked(session), "e2e4", std::nullopt, 30));
    ASSERT_TRUE(session.play(uci(session, "g2g4")));
    session.answer(answerTo(asked(session), "g2g4", std::nullopt, -120));
    ASSERT_EQ(session.phase(), Phase::Wrong);

    session.showSolution();
    answeredFromMemory(session);
    EXPECT_EQ(session.phase(), Phase::Solved);
    EXPECT_EQ(session.game().history().back().move.uci(), "e2e4");
}

TEST(PuzzleSession, WithoutAnEngine) {
    // A mate problem: a move that is not mate cannot be judged.
    PuzzleSession mate = opened(kMorphy, 2, false);
    ASSERT_TRUE(mate.play(uci(mate, "a1a6")));
    EXPECT_EQ(mate.phase(), Phase::Wrong);
    EXPECT_EQ(mate.note().kind, Note::Kind::NoEngine);

    // A best-move problem is not prepared, and any move ends it (not as solved).
    PuzzleSession best = opened(kStart, 0, false);
    EXPECT_EQ(best.phase(), Phase::Ready);
    ASSERT_TRUE(best.play(uci(best, "e2e4")));
    EXPECT_EQ(best.phase(), Phase::Solved);
    const auto events = best.takeEvents();
    ASSERT_FALSE(events.empty());
    EXPECT_FALSE(events.back().byPlayer);

    // And neither a hint nor the solution is offered.
    PuzzleSession again = opened(kMorphy, 2, false);
    again.hint();
    again.showSolution();
    EXPECT_FALSE(again.toEngine().search.has_value());
    EXPECT_FALSE(again.helped());
}

TEST(PuzzleSession, AMoveThatEndsTheGameWithoutMate) {
    // White to move, mate in two expected; Qc7 stalemates.
    PuzzleSession session = opened("k7/3Q4/1K6/8/8/8/8/8 w - - 0 1", 2);
    ASSERT_TRUE(session.play(uci(session, "d7c7")));
    EXPECT_EQ(session.phase(), Phase::Wrong);
    EXPECT_EQ(session.note().kind, Note::Kind::GameOver);
    EXPECT_EQ(session.note().outcome, Outcome::Stalemate);
}

TEST(PuzzleSession, ClosingAbandonsTheQuestion) {
    PuzzleSession session = opened(kMorphy, 2);
    ASSERT_TRUE(session.play(uci(session, "a1a6")));
    const SearchRequest check = asked(session);
    session.close();
    EXPECT_TRUE(session.toEngine().cancel);
    session.answer(answerTo(check, "a1a6", 2));
    EXPECT_FALSE(session.toEngine().search.has_value());
    EXPECT_FALSE(session.playerToMove());
}

TEST(PuzzleSession, OnlyALegalMoveOnThePlayersTurn) {
    PuzzleSession session = opened(kMorphy, 2);
    Move illegal;
    illegal.from = parseSquare("a1");
    illegal.to   = parseSquare("b2");
    EXPECT_FALSE(session.play(illegal));
    EXPECT_EQ(session.game().plies(), 0u);

    ASSERT_TRUE(session.play(uci(session, "a1a6")));
    EXPECT_FALSE(session.playerToMove());  // the move is being checked
    Move defence;
    defence.from = parseSquare("b7");
    defence.to   = parseSquare("a6");
    EXPECT_FALSE(session.play(defence));   // and the defence is not the player's to make
    EXPECT_EQ(session.game().plies(), 1u);
}
