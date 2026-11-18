// The engine that watches a game: how it stands, what each move was worth,
// and the best move, for a hint.
//
// It looks at the position on the board first, as it comes. Once that is
// done — and only then — it goes back over the positions the game left
// before it had a proper look at them (GameReview::nextToReview), so that
// every move gets its mark. A hint is its own best move in the position on
// the board: asked for before it has one, it comes the moment it has.
//
// What it gives up and what it does not: when the game moves on, a search of
// the position that was on the board is abandoned for the new one — the bar
// should follow the board at once. A second look at an earlier position is
// let finish first: it takes a fraction of a second, and cut short it would
// only have to be run again from the start.
//
// Like PuzzleSession and Opponent it decides what to ask and does not ask
// it: the scene
//
//   - tells it when the game changes (follow), and when its engine comes up
//     or goes (setEngineUsable);
//   - does with the engine what toEngine() says, filling in the engine's
//     options, and passes it what the engine says along the way (progress)
//     and at the end (answer);
//   - shows what it hands over: takeStanding() for the bar,
//     takeMarksChanged() for the marks, takeHint() for the hint.
//
// The engine can be lent to someone else for a while (the problems mode):
// lend() keeps what the search in progress had found and asks nothing more
// until follow() is called again.
//
// It can be followed in the tests without an engine:
// engine/session/tests/WatcherTest.cpp.
#pragma once

#include "chess/Engine.h"
#include "chess/Game.h"
#include "chess/GameReview.h"
#include "chess/Judge.h"

#include <optional>
#include <string>

namespace chess {

class Watcher {
public:
    // The position on the board: for this long, or this deep, whichever
    // comes first. What it says along the way is shown from this depth on;
    // shallower, it is mostly noise.
    static constexpr int kBoardMs       = 1000;
    static constexpr int kBoardDepth    = 22;
    static constexpr int kShowFromDepth = 6;

    // A new game: what was found about the last one is no use.
    void newGame();

    // The game as it now stands, after a move, a take-back or a resignation,
    // or once the engine is back from being lent. It turns to the position on
    // the board — at once, or as soon as a second look under way is over.
    void follow(const Game& game);

    // There is an engine to ask, or there is not (any more). One coming up
    // turns at once to the position on the board.
    void setEngineUsable(bool usable);
    bool engineUsable() const { return _usable; }

    // What to do with the engine, given once: abandon the search in progress
    // if `cancel`, then start `search` if there is one. Its `tag` is the
    // watcher's own; the engine options are the caller's to fill in.
    struct ToEngine {
        bool                         cancel = false;
        std::optional<SearchRequest> search;
    };
    ToEngine toEngine();

    // What the engine says while it searches (Engine::progress), and its
    // last word (Engine::poll). Anything but the latest question's is ignored.
    void progress(const SearchResult& result);
    void answer(const SearchResult& result);

    // The engine is someone else's for a while: what the search in progress
    // had found is kept, and nothing more is asked until follow().
    void lend();

    // The person would like a hint in the position on the board. It comes
    // through takeHint(), now or once the engine has its best move.
    void wantHint();
    bool hintWanted() const { return _hintWanted; }

    // What has changed for the screen since it last asked; each once.
    std::optional<Standing>    takeStanding();      // the bar; never Unknown
    bool                       takeMarksChanged();  // the marks (review().marks())
    std::optional<std::string> takeHint();          // a move, UCI

    const GameReview& review() const { return _review; }

private:
    // Turns to the position on the board: from what is known, or by asking.
    void lookAtTheBoard();
    // With the board done, the next position worth a second look.
    void reviewNext();
    void ask(SearchRequest request, bool review);
    void withdraw();
    // A search of the position on the board, rather than one gone back over?
    bool aboutTheBoard() const;
    void boardIsDone(const std::string& best, const Score& score);
    void giveHint();
    void keepProgress();
    void stand(const Standing& standing);

    Game       _game;
    bool       _following = false;  // has it been given a game yet?
    GameReview _review;
    bool       _usable = false;

    // The question out now, and what the engine has said about it so far.
    int                         _tag = 0, _nextTag = 1;
    std::string                 _asked;  // its key in _review
    bool                        _reviewing = false;  // it is a second look, not the board's
    std::optional<SearchResult> _lastProgress;
    std::optional<SearchRequest> _request;
    bool                         _cancel = false;

    // The position on the board, once looked at to the end.
    bool        _boardDone = false;
    std::string _boardBest;  // UCI
    bool        _hintWanted = false;

    std::optional<Standing>    _standing;
    bool                       _marksChanged = false;
    std::optional<std::string> _hint;
};

}  // namespace chess
