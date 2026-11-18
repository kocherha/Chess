// Solving a problem: the position, the goal, the moves made so far, and what
// is going on — the player's turn, a move being checked, the defence on its
// way, a wrong move about to be taken back, the problem solved.
//
// A problem carries no solution. Stockfish is the arbiter: a move is right if
// the engine still finds the mate in the moves that are left (or, in a
// best-move problem, if it is as good as the engine's own choice), and the
// defence is the engine's best. This class decides what to ask the engine and
// what each answer means; it does not talk to an engine itself, and it knows
// nothing of windows. The scene that shows it does three things:
//
//   - passes on what the player does: play(), takeBack(), hint(), showSolution();
//   - does with the engine what toEngine() says — cancel the search in
//     progress, start a new one, or both, in that order; or, for a question
//     answered before, hand the `known` answer straight back — and passes
//     every answer to answer(): the session recognises its own by the tag,
//     and its tags are positive, so a caller with questions of its own can
//     tell them apart by giving those negative ones;
//   - calls tick() with the time that has passed, and after any of these
//     draws what takeEvents() reports and what phase() and note() say.
//
// Everything that happens here can be followed in the tests with a pretend
// engine: engine/session/tests/PuzzleSessionTest.cpp.
#pragma once

#include "chess/Engine.h"
#include "chess/Game.h"
#include "chess/Judge.h"

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace chess {

class PuzzleSession {
public:
    enum class Phase {
        Preparing,  // best-move problems: the engine is finding the best move first
        Ready,      // the solving side to move — the player, or the engine showing the solution
        Checking,   // the engine is judging the move just made
        Replying,   // the engine is choosing the defence, or it is about to be played
        Wrong,      // a wrong move is on the board, about to be taken back
        Solved,     // done: by the player, or shown
    };

    // The line under the headline: what is to be said about the last thing
    // that happened. Moves are in English SAN; the words are the scene's.
    struct Note {
        enum class Kind {
            None,
            Right,          // `move` keeps the mate
            NoMate,         // after `move` there is no mate in `movesLeft`
            BestRight,      // `move` is the best, or as good (`best`: the engine's, if it differs)
            BestWrong,      // `move` is clearly worse than `best`
            GameOver,       // `move` ended the game without mate: `outcome`
            NoEngine,       // there is no engine to judge with
            Hint,           // the engine would play `move`
            LookingForHint,
            SolvedAlone,
            SolvedHelped,   // solved, after a hint or with the solution shown once
            SolutionShown,
        };
        Kind        kind = Kind::None;
        std::string move, best;
        int         movesLeft = 0;
        Outcome     outcome   = Outcome::Ongoing;

        bool operator==(const Note&) const = default;
    };

    // What happened, for the board and the log: in order, since the last
    // takeEvents().
    struct Event {
        enum class Kind {
            Opened,     // a problem was set up
            Moved,      // the solving side moved (`move`, `slide`; `bySolution` when the engine did)
            Replied,    // the defence was played (`move`)
            Wrong,      // the last move was judged wrong
            TakenBack,  // moves were taken back (`automatic`: a wrong one, by itself)
            Finished,   // solved (`byPlayer`, `helped`)
        };
        Kind        kind = Kind::Opened;
        std::string move;  // UCI
        bool slide = true, bySolution = false, automatic = false, byPlayer = false, helped = false;
    };

    // How long things take. Public for the tests and for anyone tuning them.
    static constexpr float  kReplyPause   = 0.45f;  // seconds before the defence is played
    static constexpr float  kWrongSeconds = 1.8f;   // a wrong move stays on the board this long
    static constexpr int    kCheckMs      = 1500;   // does this move keep the mate?
    static constexpr int    kRecheckMs    = 1000;   // the same question, asked of the defence
    static constexpr int    kReplyMs      = 700;    // the defence
    static constexpr int    kHintMs       = 3000;   // a hint, and each move of a shown solution
    static constexpr int    kBestMs       = 2500;   // best-move problems: the best move, and the player's
    static constexpr int    kBestDepth    = 18;
    // A move is as good as the best if it gives away less than this much of
    // the chance to win (see chess::winChance).
    static constexpr double kBestTolerance = 0.03;

    // --- setting up -------------------------------------------------------------------

    // A problem: the position and a mate in so many moves, or 0 for "find the
    // best move". False, and nothing changes, if the FEN is not a legal
    // position (`error` says why).
    bool open(const std::string& fen, int mateIn, std::string* error = nullptr);
    // The same problem again from its first move.
    void restart();
    // Leaves the problem: whatever was asked of the engine is abandoned, and
    // its answer will be ignored.
    void close();

    // Whether there is an engine to ask. Without one a move in a mate problem
    // cannot be judged, and a best-move problem is not prepared.
    void setEngineUsable(bool usable) { _engineUsable = usable; }

    // --- what the player does -----------------------------------------------------------

    // A move by the player. False if it is not the player's turn or not legal.
    // `slide`: whether the board should show it moving (false for a piece the
    // player carried there).
    bool play(const Move& move, bool slide = true);
    // Back to the solving side's previous turn: the reply and the move before
    // it, or just the move if it was the last one played.
    void takeBack();
    void hint();
    // The engine plays the solving side to the end. From a solved or failed
    // problem, from the start again.
    void showSolution();

    // --- the engine ---------------------------------------------------------------------

    // What to do with the engine, given once: first abandon the search in
    // progress if `cancel`, then start `search` if there is one. The search's
    // `tag` is the session's own; the engine options are the caller's.
    //
    // A question the engine has answered before — the same move tried again,
    // a hint asked twice, a problem started over — is not asked again: its
    // answer comes back as `known`, to be handed straight to answer() (which
    // may ask something else in turn: call toEngine() again until there is
    // nothing more).
    struct ToEngine {
        bool                         cancel = false;
        std::optional<SearchRequest> search;
        std::optional<SearchResult>  known;
    };
    ToEngine toEngine();
    // An answer from the engine. Answers to anything but the latest question
    // are ignored.
    void answer(const SearchResult& result);

    // --- time ---------------------------------------------------------------------------

    void tick(float seconds);
    // Will something happen by itself before long (a reply, a take-back)?
    bool timerRunning() const {
        return !_attempt.replyMove.empty() || (_attempt.phase == Phase::Wrong && _attempt.wrongFor > 0.0f);
    }

    // --- what there is to show --------------------------------------------------------------

    std::vector<Event> takeEvents();
    bool hasEvents() const { return !_events.empty(); }

    bool          isOpen() const { return _open; }
    int           mateIn() const { return _mateIn; }
    const Game&   game() const { return _game; }
    Color         attacker() const { return _attacker; }
    Phase         phase() const { return _attempt.phase; }
    const Note&   note() const { return _attempt.note; }
    bool          showingSolution() const { return _attempt.autoSolve; }
    bool          helped() const { return _attempt.helped; }
    // One per move: "!" for the solver's good ones, "?" for the wrong one.
    const std::vector<Mark>& marks() const { return _attempt.marks; }
    // The hint, as squares; kNoSquare when there is none.
    Square hintFrom() const { return _attempt.hintFrom; }
    Square hintTo() const { return _attempt.hintTo; }

    int  attackerMoves() const;  // moves the solving side has made so far
    int  movesLeft() const;      // of the mate
    bool playerToMove() const;
    // Busy with the engine or the defence: nothing to take back or show yet.
    bool busy() const {
        const Phase now = _attempt.phase;
        return now == Phase::Checking || now == Phase::Replying || now == Phase::Preparing;
    }

private:
    enum class Question { None, Prepare, Check, Recheck, Reply, Hint };

    void ask(Question question, SearchRequest request);
    // The two questions asked from more than one place: the attacker's move
    // that mates in the moves that are left (a hint, or the solution going
    // on), and the defence's reply to the move just played.
    void askForTheMate();
    void askForTheDefence();
    void dropQuestion();
    void moved(const Move& move, bool slide);
    void judgeMate(const SearchResult& result);
    void judgeBest(const SearchResult& result);
    void reply(const std::string& uci);
    void wrong(Note why);
    void finish(bool byPlayer);
    void emit(Event event) { _events.push_back(std::move(event)); }
    std::string lastSan() const;

    bool        _open = false;
    std::string _fen;
    int         _mateIn = 0;
    bool        _engineUsable = false;

    Game  _game;
    Color _attacker = Color::White;
    // One try at the problem, from opening it (or starting over) to now.
    // Starting over is `_attempt = {}`: nothing of the last try can be left
    // behind by a field someone forgot to reset.
    struct Attempt {
        Phase phase     = Phase::Ready;
        Note  note;
        bool  autoSolve = false;  // the engine plays the solving side too: "show the solution"
        bool  helped    = false;  // a hint or the solution was asked for
        std::vector<Mark> marks;
        Square hintFrom = kNoSquare, hintTo = kNoSquare;

        // Best-move problems: the engine's choice and its worth.
        std::string bestMove;
        Score       bestScore;
        std::string checkMove;  // the move being judged, UCI

        // The defence, waiting to be played; and the wrong move's time on the board.
        std::string replyMove;
        float       replyDelay = 0.0f;
        float       wrongFor   = 0.0f;
    };
    Attempt _attempt;

    // The engine.
    Question                     _question = Question::None;
    int                          _tag = 0, _nextTag = 1;
    std::optional<SearchRequest> _request;
    std::optional<SearchResult>  _known;     // the answer to _question, from _answers
    bool                         _cancel = false;
    // Every answer the engine has given this session, by the question.
    // Kept across problems and restarts: it is the restarts it is for.
    std::map<std::string, SearchResult> _answers;
    std::string                         _asked;  // the key of the question out now

    std::vector<Event> _events;
};

}  // namespace chess
