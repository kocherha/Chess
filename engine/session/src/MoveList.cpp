#include "chess/MoveList.h"

#include <algorithm>
#include <cmath>

namespace chess {

bool MoveList::setMoves(const std::vector<std::string>& moves, bool whiteFirst, int firstNumber) {
    firstNumber = std::max(1, firstNumber);
    if (moves == _moves && whiteFirst == _whiteFirst && firstNumber == _firstNumber)
        return false;
    // A new move, or a move taken back, means the player is looking at the
    // present: go back to following the end of the list.
    _moves       = moves;
    _whiteFirst  = whiteFirst;
    _firstNumber = firstNumber;
    _following   = true;
    _scrollCarry = 0.0f;
    // What the pointer was over, or had hold of, is not there any more.
    _hovered = _pressed = _ringed = -1;
    settle();
    return true;
}

bool MoveList::setMarks(const std::vector<Mark>& marks) {
    if (marks == _marks)
        return false;
    _marks = marks;
    return true;
}

bool MoveList::setResult(const std::string& result) {
    if (result == _result)
        return false;
    _result = result;
    // The end of the game is news: show it, wherever the list had been left.
    if (!_result.empty())
        _following = true;
    settle();
    return true;
}

void MoveList::setVisibleRows(int rows) {
    _visible = std::max(1, rows);
    settle();
}

Mark MoveList::mark(int index) const {
    return index >= 0 && static_cast<std::size_t>(index) < _marks.size() ? _marks[static_cast<std::size_t>(index)]
                                                                          : Mark::None;
}

int MoveList::rowCount() const {
    const int result = _result.empty() ? 0 : 1;
    if (_moves.empty())
        return result;
    const std::size_t cells = _moves.size() + (_whiteFirst ? 0 : 1);
    return static_cast<int>((cells + 1) / 2) + result;
}

int MoveList::moveIn(int row, int side) const {
    if (row < 0 || isResultRow(row))
        return -1;
    const int index = row * 2 + side - (_whiteFirst ? 0 : 1);
    return index >= 0 && index < moveCount() ? index : -1;
}

int MoveList::lastFirstRow() const {
    return std::max(0, rowCount() - _visible);
}

bool MoveList::scroll(float rows) {
    _scrollCarry += rows;
    const int whole = static_cast<int>(_scrollCarry);  // towards zero: the remainder keeps its sign
    if (whole == 0)
        return false;
    _scrollCarry -= static_cast<float>(whole);

    const int last = lastFirstRow();
    const int next = std::clamp(_firstRow - whole, 0, last);
    _following     = next == last;
    if (next == _firstRow) {
        // Up against one end: nothing is owed in that direction any more.
        _scrollCarry = 0.0f;
        return false;
    }
    _firstRow = next;
    return true;
}

bool MoveList::scrollTo(float share) {
    const int last = lastFirstRow();
    const int next = static_cast<int>(std::lround(std::clamp(share, 0.0f, 1.0f) * static_cast<float>(last)));
    _following     = next == last;
    _scrollCarry   = 0.0f;
    if (next == _firstRow)
        return false;
    _firstRow = next;
    return true;
}

bool MoveList::setHovered(int index) {
    if (index == _hovered)
        return false;
    _hovered = index;
    return true;
}

std::optional<int> MoveList::release(int index) {
    const int pressed = _pressed;
    _pressed          = -1;
    if (pressed >= 0 && index == pressed)
        return pressed;
    return std::nullopt;
}

bool MoveList::setRinged(int index) {
    if (index == _ringed)
        return false;
    _ringed = index;
    return true;
}

void MoveList::settle() {
    const int last = lastFirstRow();
    _firstRow      = _following ? last : std::clamp(_firstRow, 0, last);
}

}  // namespace chess
