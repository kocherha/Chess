#include "chess/Difficulty.h"
#include "chess/Engine.h"
#include "chess/Game.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <mutex>
#include <thread>

#include <unistd.h>

using namespace chess;
using namespace std::chrono_literals;

namespace {

template <typename Predicate>
bool waitFor(Predicate done, std::chrono::milliseconds limit = 5000ms) {
    const auto deadline = std::chrono::steady_clock::now() + limit;
    while (!done()) {
        if (std::chrono::steady_clock::now() > deadline)
            return false;
        std::this_thread::sleep_for(2ms);
    }
    return true;
}

bool waitForResult(Engine& engine, SearchResult& result, std::chrono::milliseconds limit = 5000ms) {
    return waitFor([&] { return engine.poll(result); }, limit);
}

// Collects what the client sends, for the tests that care about the wire.
struct Wire {
    std::mutex               mutex;
    std::vector<std::string> sent;

    void attach(Engine& engine) {
        engine.setTrace([this](char direction, const std::string& line) {
            if (direction == '>') {
                std::lock_guard lock(mutex);
                sent.push_back(line);
            }
        });
    }
    std::vector<std::string> lines() {
        std::lock_guard lock(mutex);
        return sent;
    }
    int count(const std::string& line) {
        const auto all = lines();
        return static_cast<int>(std::count(all.begin(), all.end(), line));
    }
    int indexOf(const std::string& line) {
        const auto all  = lines();
        const auto found = std::find(all.begin(), all.end(), line);
        return found == all.end() ? -1 : static_cast<int>(found - all.begin());
    }
};

SearchRequest quick(int tag = 0) {
    SearchRequest request;
    request.position   = "position startpos";
    request.moveTimeMs = 100;
    request.tag        = tag;
    return request;
}

// The fake engine's "go movetime 60000": does not answer until told to stop.
SearchRequest endless(int tag = 0) {
    SearchRequest request = quick(tag);
    request.moveTimeMs    = 60000;
    return request;
}

}  // namespace

TEST(FakeEngine, Handshake) {
    Engine engine;
    ASSERT_TRUE(engine.start(CHESS_FAKE_ENGINE));
    ASSERT_TRUE(waitFor([&] { return engine.state() == Engine::State::Ready; }));

    EXPECT_EQ(engine.name(), "Fake Engine 1.0");
    EXPECT_TRUE(engine.hasOption("UCI_LimitStrength"));
    EXPECT_TRUE(engine.hasOption("Skill Level"));  // a name with a space in it
    EXPECT_FALSE(engine.hasOption("Hash"));

    const auto elo = engine.spin("UCI_Elo");
    ASSERT_TRUE(elo.has_value());
    EXPECT_EQ(elo->min, 1320);
    EXPECT_EQ(elo->max, 3190);
    EXPECT_FALSE(engine.spin("UCI_LimitStrength").has_value());  // a check box, not a number
    EXPECT_FALSE(engine.thinking());
}

TEST(FakeEngine, AnswersASearch) {
    Engine engine;
    ASSERT_TRUE(engine.start(CHESS_FAKE_ENGINE));

    // Asked before the handshake can possibly be over: the request waits.
    engine.search(quick(42));
    EXPECT_TRUE(engine.thinking());

    SearchResult result;
    ASSERT_TRUE(waitForResult(engine, result));
    EXPECT_EQ(result.tag, 42);
    EXPECT_EQ(result.bestMove, "e2e4");
    ASSERT_TRUE(result.scoreCp.has_value());
    EXPECT_EQ(*result.scoreCp, 31);
    EXPECT_FALSE(result.mateIn.has_value());
    EXPECT_EQ(result.depth, 7);

    EXPECT_FALSE(engine.thinking());
    EXPECT_FALSE(engine.poll(result));  // exactly one answer
}

TEST(FakeEngine, ANewSearchReplacesTheOldOne) {
    Wire wire;  // declared first: it must outlive the engine that reports to it
    Engine engine;
    wire.attach(engine);
    ASSERT_TRUE(engine.start(CHESS_FAKE_ENGINE));
    ASSERT_TRUE(waitFor([&] { return engine.state() == Engine::State::Ready; }));

    engine.search(endless(1));
    engine.search(quick(2));

    SearchResult result;
    ASSERT_TRUE(waitForResult(engine, result));
    EXPECT_EQ(result.tag, 2);
    EXPECT_EQ(result.bestMove, "e2e4");

    // The abandoned search was stopped, its move (a1a2) went nowhere, and the
    // second `go` only left after the first had been answered.
    std::this_thread::sleep_for(50ms);
    EXPECT_FALSE(engine.poll(result));
    EXPECT_EQ(wire.count("stop"), 1);
    EXPECT_LT(wire.indexOf("go movetime 60000"), wire.indexOf("stop"));
    EXPECT_LT(wire.indexOf("stop"), wire.indexOf("go movetime 100"));
}

TEST(FakeEngine, CancelMeansNoAnswer) {
    Engine engine;
    ASSERT_TRUE(engine.start(CHESS_FAKE_ENGINE));
    ASSERT_TRUE(waitFor([&] { return engine.state() == Engine::State::Ready; }));

    engine.search(endless(1));
    EXPECT_TRUE(engine.thinking());
    engine.cancel();
    EXPECT_FALSE(engine.thinking());

    SearchResult result;
    EXPECT_FALSE(waitForResult(engine, result, 150ms));

    // And the engine is still good for the next question.
    engine.search(quick(2));
    ASSERT_TRUE(waitForResult(engine, result));
    EXPECT_EQ(result.tag, 2);
}

TEST(FakeEngine, MateScoresAreNotCentipawns) {
    Engine engine;
    ASSERT_TRUE(engine.start(CHESS_FAKE_ENGINE));
    ASSERT_TRUE(waitFor([&] { return engine.state() == Engine::State::Ready; }));

    // Let the endless search be answered properly this time: stop it by hand
    // with a second request that we then also let finish.
    engine.search(endless(1));
    std::this_thread::sleep_for(20ms);
    engine.search(quick(2));
    SearchResult result;
    ASSERT_TRUE(waitForResult(engine, result));
    EXPECT_EQ(result.tag, 2);
    EXPECT_TRUE(result.scoreCp.has_value());
    EXPECT_FALSE(result.mateIn.has_value());  // nothing carried over from "mate -2"
}

TEST(FakeEngine, ProgressIsTheBestLineAndNothingElse) {
    Engine engine;
    ASSERT_TRUE(engine.start(CHESS_FAKE_ENGINE));

    SearchResult seen;
    EXPECT_FALSE(engine.progress(seen));  // nothing is being searched

    engine.search(endless(5));
    ASSERT_TRUE(waitFor([&] { return engine.progress(seen) && seen.depth == 9; }));
    // Give the lines that must be ignored time to arrive and be ignored.
    std::this_thread::sleep_for(60ms);
    ASSERT_TRUE(engine.progress(seen));

    EXPECT_EQ(seen.tag, 5);
    EXPECT_TRUE(seen.bestMove.empty());  // still thinking
    EXPECT_EQ(seen.pvMove, "d2d4");
    EXPECT_EQ(seen.depth, 9);            // not the bound at depth 10
    ASSERT_TRUE(seen.scoreCp.has_value());
    EXPECT_EQ(*seen.scoreCp, 55);        // not 140 (a bound), not -300 (the second line)
    EXPECT_FALSE(seen.mateIn.has_value());  // not the "mate 1" in the free text

    // The second line is kept too, as what it is.
    EXPECT_EQ(seen.secondMove, "a2a3");
    EXPECT_EQ(seen.secondDepth, 9);
    ASSERT_TRUE(seen.secondCp.has_value());
    EXPECT_EQ(*seen.secondCp, -300);
    EXPECT_FALSE(seen.secondMate.has_value());

    // An abandoned search has nothing to report to anyone.
    engine.cancel();
    EXPECT_FALSE(engine.progress(seen));
}

TEST(FakeEngine, OptionsAreSentOnceAndOnlyIfKnown) {
    Wire wire;  // declared first: it must outlive the engine that reports to it
    Engine engine;
    wire.attach(engine);
    ASSERT_TRUE(engine.start(CHESS_FAKE_ENGINE));

    SearchRequest request = quick();
    request.options       = {{"UCI_LimitStrength", "true"}, {"UCI_Elo", "1600"}, {"NoSuchOption", "1"}};
    SearchResult result;

    engine.search(request);
    ASSERT_TRUE(waitForResult(engine, result));
    engine.search(request);  // the same again: nothing has changed
    ASSERT_TRUE(waitForResult(engine, result));
    request.options[1].second = "1900";
    engine.search(request);
    ASSERT_TRUE(waitForResult(engine, result));

    EXPECT_EQ(wire.count("setoption name UCI_LimitStrength value true"), 1);
    EXPECT_EQ(wire.count("setoption name UCI_Elo value 1600"), 1);
    EXPECT_EQ(wire.count("setoption name UCI_Elo value 1900"), 1);
    EXPECT_EQ(wire.count("setoption name NoSuchOption value 1"), 0);
    EXPECT_EQ(wire.count("position startpos"), 3);
}

TEST(FakeEngine, NewGameIsAnnouncedBeforeTheNextPosition) {
    Wire wire;  // declared first: it must outlive the engine that reports to it
    Engine engine;
    wire.attach(engine);
    ASSERT_TRUE(engine.start(CHESS_FAKE_ENGINE));

    SearchResult result;
    engine.search(quick());
    ASSERT_TRUE(waitForResult(engine, result));
    EXPECT_EQ(wire.count("ucinewgame"), 0);

    engine.newGame();
    engine.search(quick());
    ASSERT_TRUE(waitForResult(engine, result));
    engine.search(quick());
    ASSERT_TRUE(waitForResult(engine, result));

    EXPECT_EQ(wire.count("ucinewgame"), 1);  // once, not before every move
    const auto lines = wire.lines();
    const auto at    = std::find(lines.begin(), lines.end(), "ucinewgame");
    ASSERT_NE(at + 1, lines.end());
    EXPECT_EQ(*(at + 1), "position startpos");
}

TEST(FakeEngine, DepthAndTimeLimits) {
    Wire wire;  // declared first: it must outlive the engine that reports to it
    Engine engine;
    wire.attach(engine);
    ASSERT_TRUE(engine.start(CHESS_FAKE_ENGINE));

    SearchResult result;
    SearchRequest request = quick();
    request.depth         = 6;
    engine.search(request);
    ASSERT_TRUE(waitForResult(engine, result));
    request.moveTimeMs = 0;
    engine.search(request);
    ASSERT_TRUE(waitForResult(engine, result));
    request.depth = 0;  // no limit given: the client must not send a bare `go`
    engine.search(request);
    ASSERT_TRUE(waitForResult(engine, result));

    EXPECT_EQ(wire.count("go movetime 100 depth 6"), 1);
    EXPECT_EQ(wire.count("go depth 6"), 1);
    EXPECT_EQ(wire.count("go movetime 1000"), 1);
    EXPECT_EQ(wire.count("go"), 0);
}

TEST(FakeEngine, AnEngineThatDiesIsReportedNotWaitedFor) {
    Engine engine;
    ASSERT_TRUE(engine.start(CHESS_FAKE_ENGINE));
    ASSERT_TRUE(waitFor([&] { return engine.state() == Engine::State::Ready; }));

    SearchRequest fatal = quick();
    fatal.moveTimeMs    = 0;
    fatal.depth         = 99;  // the fake engine's cue to exit
    engine.search(fatal);

    ASSERT_TRUE(waitFor([&] { return engine.state() == Engine::State::Failed; }));
    EXPECT_FALSE(engine.thinking());
    EXPECT_FALSE(engine.error().empty());

    // Asking a dead engine is harmless...
    engine.search(quick());
    EXPECT_FALSE(engine.thinking());

    // ...and it can be started again.
    ASSERT_TRUE(engine.start(CHESS_FAKE_ENGINE));
    engine.search(quick(7));
    SearchResult result;
    ASSERT_TRUE(waitForResult(engine, result));
    EXPECT_EQ(result.tag, 7);
}

TEST(FakeEngine, MissingProgram) {
    Engine engine;
    EXPECT_FALSE(engine.start("/nonexistent/stockfish"));
    EXPECT_EQ(engine.state(), Engine::State::Failed);
    EXPECT_FALSE(engine.error().empty());
    EXPECT_FALSE(engine.thinking());
}

TEST(FakeEngine, ShutdownWhileThinkingDoesNotHang) {
    Engine engine;
    ASSERT_TRUE(engine.start(CHESS_FAKE_ENGINE));
    engine.search(endless());
    std::this_thread::sleep_for(20ms);

    const auto before = std::chrono::steady_clock::now();
    engine.shutdown();
    EXPECT_LT(std::chrono::steady_clock::now() - before, 2s);
    EXPECT_EQ(engine.state(), Engine::State::Stopped);
}

TEST(FindEngine, EnvironmentWinsThenFallback) {
    ::setenv("CHESS_ENGINE", CHESS_FAKE_ENGINE, 1);
    EXPECT_EQ(findEngine("/bin/sh"), CHESS_FAKE_ENGINE);

    // Something that is not there is not an answer; move on to the next place.
    ::setenv("CHESS_ENGINE", "/nonexistent/engine", 1);
    EXPECT_EQ(findEngine("/bin/sh"), "/bin/sh");
    ::unsetenv("CHESS_ENGINE");
    EXPECT_EQ(findEngine("/bin/sh"), "/bin/sh");

    EXPECT_FALSE(executableDirectory().empty());
}

// --- against the real engine -------------------------------------------------------

class Stockfish : public ::testing::Test {
protected:
    void SetUp() override {
        if (::access(CHESS_REAL_ENGINE, X_OK) != 0)
            GTEST_SKIP() << "no Stockfish at " << CHESS_REAL_ENGINE;
        ASSERT_TRUE(engine.start(CHESS_REAL_ENGINE));
        ASSERT_TRUE(waitFor([&] { return engine.state() == Engine::State::Ready; }, 20000ms));
    }

    SearchResult ask(const Game& game, const Level& level) {
        const auto elo = engine.spin("UCI_Elo");
        SearchRequest request;
        request.position   = game.uciPosition();
        request.moveTimeMs = level.moveTimeMs;
        request.options    = engineOptions(level, elo ? elo->min : 0, elo ? elo->max : 0, 2);
        engine.search(request);
        SearchResult result;
        EXPECT_TRUE(waitForResult(engine, result, 30000ms));
        return result;
    }

    Engine engine;
};

TEST_F(Stockfish, IntroducesItself) {
    EXPECT_NE(engine.name().find("Stockfish"), std::string::npos) << engine.name();
    // The two options every difficulty level depends on.
    EXPECT_TRUE(engine.hasOption("UCI_LimitStrength"));
    const auto elo = engine.spin("UCI_Elo");
    ASSERT_TRUE(elo.has_value());
    EXPECT_LE(elo->min, 1400);
    EXPECT_GE(elo->max, 2800);
}

TEST_F(Stockfish, FindsMateInOne) {
    Game game(*Position::fromFen("6k1/5ppp/8/8/8/8/8/R3K3 w - - 0 1"));
    const SearchResult result = ask(game, levels().back());
    EXPECT_EQ(result.bestMove, "a1a8");
    ASSERT_TRUE(result.mateIn.has_value());
    EXPECT_EQ(*result.mateIn, 1);

    ASSERT_TRUE(game.playUci(result.bestMove));
    EXPECT_EQ(game.status().outcome, Outcome::Checkmate);
}

TEST_F(Stockfish, HasNoMoveWhenTheGameIsOver) {
    Game mated(*Position::fromFen("R5k1/5ppp/8/8/8/8/8/4K3 b - - 0 1"));
    ASSERT_EQ(mated.status().outcome, Outcome::Checkmate);
    EXPECT_TRUE(ask(mated, levelByNumber(3)).bestMove.empty());
}

// The agreement that matters most: every move the engine plays, at every
// level, must be one our own rules consider legal — and we must agree with it
// about when the game is over. Two short games per level, engine against
// itself, with the history passed the way the app passes it.
TEST_F(Stockfish, EveryLevelPlaysLegalChess) {
    for (const Level& configured : levels()) {
        Level level      = configured;
        level.moveTimeMs = 20;  // legality does not need thinking time

        Game game;
        engine.newGame();
        for (int ply = 0; ply < 60 && !game.status().over(); ++ply) {
            const SearchResult result = ask(game, level);
            ASSERT_FALSE(result.bestMove.empty())
                << "level " << level.number << ": no move in " << game.position().fen();
            ASSERT_TRUE(game.playUci(result.bestMove))
                << "level " << level.number << ": " << result.bestMove << " is not legal in "
                << game.position().fen();
        }
        EXPECT_GT(game.plies(), 0u);
    }
}
