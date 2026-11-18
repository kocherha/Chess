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
// after — and deeply enough. The position on the board is looked at as it
// comes; one the game moved on from too quickly gets its turn later, once the
// engine has nothing more urgent (nextToReview). What has been found is kept
// by position, so a move taken back and played again needs no second look.
//
// This class asks nothing itself: the scene runs the searches and tells it
// what they found (note).
class GameReview {
public:
    // A position is looked at again if the game left it before the engine had
    // got this deep, for at most this long; no mark is made from anything
    // shallower than kMarkDepth, finished or not.
    static constexpr int kReviewDepth = 18;
    static constexpr int kReviewMs    = 800;
    static constexpr int kMarkDepth   = 14;

    struct Finding {
        int  depth    = 0;
        bool complete = false;  // the search ran its course instead of being cut short
        Score       score;      // for the side to move there
        std::string best;       // its first choice, UCI
        std::optional<Score> second;  // what its second choice was worth
    };

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
    // false) or at its end. Kept unless what is already known is better. True
    // if that changed the marks.
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
    std::vector<std::string>       _keys{"position startpos"};
    std::map<std::string, Finding> _findings;  // by key
    std::vector<Mark>              _marks;
};

}  // namespace chess
