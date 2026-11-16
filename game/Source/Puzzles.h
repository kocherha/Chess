// The problems there are to solve: the collection that comes with the game,
// and the positions the player has saved from the editor.
//
// A problem is a position and a goal — mate in so many moves, or, for a
// position of the player's own without a forced mate, the best move. It does
// not carry its solution. Whether a move is right is Stockfish's to say while
// the problem is being solved, so a problem typed in by hand needs nothing but
// its position, and a second solution the composer missed is accepted as
// readily as the first.
#pragma once

#include <string>
#include <vector>

struct Puzzle {
    std::string id;      // "m2-morphy" for the collection, "mine-3" for one of the player's
    std::string title;
    std::string note;    // where it comes from, if anywhere in particular
    std::string fen;
    int  mateIn = 0;     // moves; 0 for "find the best move"
    bool mine   = false;
};

namespace puzzles {

// The collection, easiest first. Every position in it was checked with
// Stockfish when it was put in: a forced mate in exactly that many moves.
const std::vector<Puzzle>& collection();

// The player's own, in the order they were saved. Kept in the game's settings.
std::vector<Puzzle> mine();
// Saves a position and returns it as a puzzle. The same position saved twice
// is one puzzle.
Puzzle save(const std::string& fen, int mateIn);
void   remove(const std::string& id);

// Solved ones are ticked in the list, and the next one offered is the first
// that is not.
bool solved(const std::string& id);
void markSolved(const std::string& id);

// Everything, collection first: what "next" walks through.
std::vector<Puzzle> all();
// The puzzle after this one, or the first if it was the last.
Puzzle next(const std::string& id);
// One by id; an empty puzzle if there is none.
Puzzle find(const std::string& id);

}  // namespace puzzles
