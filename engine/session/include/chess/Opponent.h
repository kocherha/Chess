// The computer as the other player of a game: when to answer, and how.
//
// On its turn it answers in one of three ways. A position it has answered
// before in this game — the person took a move back and played the same one
// again — gets the same answer: a weakened engine chooses with a roll of the
// dice, and without this "take back" would double as "roll again". Below the
// engine's weakest setting a share of its moves are picked at random
// (Level::randomMoveShare). Everything else is asked of the engine. A reply
// that needed no engine is still played after a pause (Pause.h).
//
// Like PuzzleSession it decides what to ask and does not ask it: the scene
//
//   - tells it when the game changes (follow) and when what it was asked is
//     no longer wanted (withdraw);
//   - does with the engine what toEngine() says — cancel the search in
//     progress, start a new one, or both, in that order — after filling in
//     the search's engine options, and passes every answer to answer();
//   - calls tick() with the time that has passed, and plays the move
//     takeMove() hands it, if any.
//
// It can be followed in the tests without an engine:
// engine/session/tests/OpponentTest.cpp.
#pragma once

#include "chess/Difficulty.h"
#include "chess/Engine.h"
#include "chess/Game.h"

#include <cstdint>
#include <map>
#include <optional>
#include <random>
#include <string>

namespace chess {

// A legal move in `position` picked at random, or none if there is none.
// Random, not perverse: a pawn that reaches the end becomes a queen.
std::optional<Move> randomMove(const Position& position, std::mt19937& rng);

class Opponent {
public:
    // `seed` for the dice; a test passes its own to know what they roll.
    explicit Opponent(std::uint32_t seed = std::random_device{}());

    // A new game, the computer playing `side` at `level`. What it answered in
    // the last one is forgotten.
    void newGame(Color side, const Level& level);

    // The game as it stands now, after a move, a take-back or a resignation.
    // On the computer's turn it decides how to answer; on the person's, or
    // once the game is over, whatever it was about to do is withdrawn.
    // `engineUsable`: there is an engine to ask (or about to be). An answer
    // that needs one when there is none means the opponent is lost.
    void follow(const Game& game, bool engineUsable);

    // What was asked or waited for is about a position that is gone.
    void withdraw();

    // What to do with the engine, given once: abandon the search in progress
    // if `cancel`, then start `search` if there is one. Its `tag` is the
    // opponent's own; the engine options are the caller's to fill in.
    struct ToEngine {
        bool                         cancel = false;
        std::optional<SearchRequest> search;
    };
    ToEngine toEngine();

    // An answer from the engine. Anything but the answer to the latest
    // question is ignored. A move that is not legal in the game it was asked
    // about means the two disagree about the rules: the opponent is lost.
    void answer(const SearchResult& result);

    // Time passing: a reply that needed no engine is played once its pause
    // is over.
    void tick(float seconds);

    // The computer's move, to be played now; once, then none until the next.
    std::optional<Move> takeMove();

    // Waiting out the pause before a reply it already has: the screen has to
    // keep drawing, or the reply would never come.
    bool pausing() const { return _pause.has_value(); }

    // It cannot go on: its engine is gone, or answered something impossible.
    // `trouble()` says which, for the log.
    void engineFailed(const std::string& why);
    bool               lost() const { return _lost; }
    const std::string& trouble() const { return _trouble; }

    Color side() const { return _side; }

private:
    // The reply waiting out its pause: a move it already knows, or — empty —
    // one to be picked at random when the pause is over.
    struct Pause {
        std::string move;  // UCI
        float       left = 0.0f;
    };

    void lose(std::string why);

    Color        _side  = Color::Black;
    Level        _level;
    Game         _game;
    std::mt19937 _rng;

    // What it answered in each position of this game, by the moves that led
    // there (Game::uciPosition()).
    std::map<std::string, std::string> _answered;

    std::optional<Pause> _pause;
    std::optional<Move>  _ready;  // decided, waiting for takeMove()

    // The engine.
    int                          _tag = 0, _nextTag = 1;
    std::string                  _asked;  // the position the question out now is about
    std::optional<SearchRequest> _request;
    bool                         _cancel = false;

    bool        _lost = false;
    std::string _trouble;
};

}  // namespace chess
