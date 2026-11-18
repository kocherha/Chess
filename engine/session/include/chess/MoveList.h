#pragma once

#include "chess/Judge.h"

#include <optional>
#include <string>
#include <vector>

namespace chess {

// The list of moves beside the board, without the drawing: which rows there
// are, what number each one carries, which move is in which cell, how far the
// list is scrolled, and which move the pointer is on.
//
// A row is one move of each side, numbered as the game record numbers them: a
// game set up from a FEN starts at its own move number, and one that began
// with black to move has an empty first cell. Once the game is over, the
// result has a row of its own after the last move.
//
// Unless the player has scrolled away, the list follows its end: a move made,
// or taken back, or the game ending, brings it back there.
class MoveList {
public:
    // The moves so far, already in the notation to show. False if nothing
    // changed.
    bool setMoves(const std::vector<std::string>& moves, bool whiteFirst = true, int firstNumber = 1);
    // What each move was worth. One per move, or fewer: they come later than
    // the moves do, and do not move the list.
    bool setMarks(const std::vector<Mark>& marks);
    // How the game ended ("1–0"); empty while it is being played.
    bool setResult(const std::string& result);
    // How many rows there is room for.
    void setVisibleRows(int rows);

    bool               empty() const { return _moves.empty() && _result.empty(); }
    int                moveCount() const { return static_cast<int>(_moves.size()); }
    const std::string& move(int index) const { return _moves[static_cast<std::size_t>(index)]; }
    Mark               mark(int index) const;
    const std::string& result() const { return _result; }
    int                newest() const { return moveCount() - 1; }

    int  rowCount() const;
    int  visibleRows() const { return _visible; }
    bool isResultRow(int row) const { return !_result.empty() && row == rowCount() - 1; }
    int  number(int row) const { return _firstNumber + row; }
    // The move in a row's cell (side 0: white's, 1: black's), or -1.
    int moveIn(int row, int side) const;

    // Scrolling. firstRow() is the first row on screen; lastFirstRow() is
    // where it stands when scrolled to the end.
    int firstRow() const { return _firstRow; }
    int lastFirstRow() const;
    // By this many rows (positive: towards the start). Fractions add up: a
    // trackpad sends its movement as many small steps, and the list moves a
    // row whenever they have come to one. True if the list moved.
    bool scroll(float rows);
    // To a share of the way from the start (0) to the end (1), as the scroll
    // bar's thumb is dragged. True if the list moved.
    bool scrollTo(float share);

    // The move under the pointer, the one the button went down on, and the
    // one ringed (just copied); -1 for none. Moves changing forgets them all.
    int  hovered() const { return _hovered; }
    bool setHovered(int index);  // true if that changed anything
    void press(int index) { _pressed = index; }
    // The button came up over `index`: a click, if it is where it went down.
    std::optional<int> release(int index);
    int  marked() const { return _marked; }
    bool setMarked(int index);   // true if that changed anything

private:
    // Puts the first row where it belongs: at the end if following it, and
    // never past either end.
    void settle();

    std::vector<std::string> _moves;
    std::vector<Mark>        _marks;
    std::string              _result;
    bool  _whiteFirst  = true;
    int   _firstNumber = 1;
    int   _visible     = 1;
    int   _firstRow    = 0;
    bool  _following   = true;
    float _scrollCarry = 0.0f;  // the part of a row scroll() has not used yet
    int   _hovered = -1, _pressed = -1, _marked = -1;
};

}  // namespace chess
