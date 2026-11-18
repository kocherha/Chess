#include "BoardInput.h"

#include "BoardNode.h"

using namespace chess;
using ax::Vec2;

bool BoardInput::promotionClick(const Vec2& point) {
    if (!_board->promotionOpen())
        return false;
    const PieceType piece = _board->promotionAt(point);
    const Square    from = _promoteFrom, to = _promoteTo;
    const bool      carried = _promoteCarried;
    _promoteFrom = _promoteTo = kNoSquare;
    _board->closePromotion();
    select(kNoSquare);
    if (piece != PieceType::None)
        if (const auto move = position().findMove(from, to, piece))
            play(*move, !carried);
    return true;
}

bool BoardInput::mouseDown(const Vec2& point) {
    const Square square = _board->squareAt(point);
    if (square == kNoSquare || !mayMove())
        return false;

    // A second click, on a square the selected piece can go to: that is the move.
    if (_selected != kNoSquare && _selected != square && tryMove(_selected, square, false))
        return true;

    const Position& now   = position();
    const Piece     piece = now.at(square);
    if (piece.empty() || piece.color != now.sideToMove()) {
        select(kNoSquare);
        return true;
    }
    _wasSelected = _selected == square;
    select(square);
    _pressed     = true;
    _dragging    = false;
    _pressSquare = square;
    _pressPoint  = point;
    return true;
}

bool BoardInput::mouseMove(const Vec2& point) {
    if (!_pressed)
        return false;
    if (!_dragging && point.distance(_pressPoint) > kDragThreshold) {
        _dragging = true;
        _board->beginDrag(_pressSquare, point);
    }
    if (_dragging)
        _board->dragTo(point);
    return true;
}

void BoardInput::mouseUp(const Vec2& point) {
    if (!_pressed)
        return;
    _pressed = false;

    if (_dragging) {
        _dragging = false;
        // The board was shown afresh under the hand (the window was resized,
        // say): the piece was put back, and there is nothing to drop.
        if (!_board->dragging())
            return;
        const Square target = _board->squareAt(point);
        _board->endDrag();
        // Dropped on a square it can go to: the move is made, and the piece is
        // already there, so it does not slide. Dropped anywhere else: it is
        // back where it was, still selected.
        if (target != kNoSquare && target != _pressSquare && mayMove())
            tryMove(_pressSquare, target, true);
    } else if (_wasSelected) {
        // A plain click on the piece that was already selected puts it down.
        select(kNoSquare);
    }
}

void BoardInput::select(Square square) {
    if (_selected == square)
        return;
    _selected = square;
    selectionChanged();
}

void BoardInput::cancelPress() {
    _pressed = _dragging = false;
    _pressSquare = kNoSquare;
    _board->endDrag();
}

void BoardInput::cancelPromotion() {
    if (!_board->promotionOpen())
        return;
    _promoteFrom = _promoteTo = kNoSquare;
    _board->closePromotion();
    select(kNoSquare);
}

void BoardInput::reset() {
    _selected    = kNoSquare;
    _promoteFrom = _promoteTo = kNoSquare;
    _board->closePromotion();
    cancelPress();
}

bool BoardInput::tryMove(Square from, Square to, bool carried) {
    const Position& now = position();

    if (now.isPromotion(from, to)) {
        // Not a move yet: the player still has to say what the pawn becomes.
        _promoteFrom    = from;
        _promoteTo      = to;
        _promoteCarried = carried;
        _board->openPromotion(to, now.sideToMove());
        return true;
    }

    const auto move = now.findMove(from, to);
    if (!move)
        return false;
    play(*move, !carried);
    return true;
}
