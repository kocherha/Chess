#pragma once

#include "chess/Engine.h"
#include "chess/Game.h"
#include "chess/Judge.h"

#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace chess {

// What the watching engine has found out about the positions of a game, and
// the marks (!!, !, ?!, ?, ??) that follow from it (see Judge.h).
//
// A mark needs two positions looked at — the one before the move and the one
// after. The position on the board is looked at as it comes; one the game
// moved on from too quickly gets its turn later, once the engine has nothing
// more urgent (nextToReview).
//
// A move is judged from what the engine said when it had looked exactly
// kJudgeDepth deep (SearchResult::noted) — not from its last word, which
// depends on how long it was let think. So the same game gets the same marks
// on a fast machine and a slow one, today and tomorrow, and a search cut
// short after that depth needs no second look.
//
// What has been found is kept by position: a move taken back and played
// again, or the same position reached by another order of moves, needs no
// second look. (A position that stands on the board for the second time in
// one game is another matter — a third time would be a draw — and is kept
// apart from its first occurrence.)
//
// This class asks nothing itself: whoever runs the searches tells it what
// they found (note).
class GameReview {
public:
    static constexpr int kJudgeDepth = 16;
    // A machine too slow to get there in time is taken at its last word, if
    // that is at least this deep; nothing shallower makes a mark.
    static constexpr int kMarkDepth = 14;
    // A second look goes to kJudgeDepth and no further, for at most this long.
    static constexpr int kReviewMs = 2500;

    // What a move is judged by: the position's score for the side to move
    // there, its best move, and what the second-best was worth.
    struct Judgement {
        int                  depth = 0;
        Score                score;
        std::string          best;  // UCI
        std::optional<Score> second;
    };
    struct Finding {
        // The engine's latest word, for the bar and the hint.
        int         depth    = 0;
        bool        complete = false;  // the search ran its course instead of being cut short
        Score       score;             // for the side to move there
        std::string best;              // its first choice, UCI
        // Set once, and then kept: see above.
        std::optional<Judgement> judged;
    };

    GameReview() { clear(); }

    // A new game: everything found so far is about another one.
    void clear();

    // The game as it now stands, after a move, a take-back or a resignation.
    // True if that changed the marks.
    bool follow(const Game& game);

    // The key of each position of the game, the starting one first: the
    // `position` command that leads there, as the engine is given it. The last
    // is the position on the board (Game::uciPosition()).
    const std::vector<std::string>& keys() const { return _keys; }
    const std::string&              now() const { return _keys.back(); }

    // A search of the position `key` has reported: on the way (complete =
    // false) or at its end. Its latest word is kept unless what is already
    // known is better; what it noted at kJudgeDepth is kept if nothing was
    // yet. True if that changed the marks.
    bool note(const std::string& key, const SearchResult& result, bool complete);
    const Finding* found(const std::string& key) const;

    // The position most worth a second look, as the search to run (its tag and
    // options left to the caller); none if every one is known well enough.
    // The most recent first: its move is the one the player is looking at.
    std::optional<SearchRequest> nextToReview() const;

    // One per move of the game; None where there is not enough to go on yet.
    const std::vector<Mark>& marks() const { return _marks; }
    Mark                     mark(std::size_t ply) const;

private:
    bool judgeMoves();

    Game                           _game;
    std::vector<std::string>       _keys;
    // Where each key's finding is filed: by the position it leads to (and
    // which time round it stands there), so that keys leading to the same
    // position share one. Kept for every key the game has had, on the board
    // now or taken back.
    std::map<std::string, std::string> _places;
    std::map<std::string, Finding>     _findings;  // by place
    std::vector<Mark>                  _marks;
};

}  // namespace chess
