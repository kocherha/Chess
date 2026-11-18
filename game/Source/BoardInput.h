#pragma once

#include "chess/Position.h"

#include "axmol.h"

#include <functional>

class BoardNode;

// The player moving pieces on the board, the same way in a game and in a
// problem: click a piece and then a square, or drag it there; a pawn that
// reaches the last rank asks, on the board, what it becomes.
//
// It decides nothing about chess: the scene says which position the player is
// moving in and whether it is their turn, and is handed the move.
//
// A press on a piece is a click (select it) or the start of a drag (carry it);
// which one is only known once the pointer has moved a little. A press is
// forgotten (cancelPress) whenever the position it was about is no longer the
// one on the board, or a dialog takes the mouse.
class BoardInput {
public:
    // How far the pointer moves, held down on a piece, before the press is a drag.
    static constexpr float kDragThreshold = 5.0f;

    // The board it works on; set once, when the scene has made it.
    void attach(BoardNode* board) { _board = board; }

    // Set by the scene.
    std::function<const chess::Position&()>             position;  // the position moves are made in
    std::function<bool()>                               mayMove;   // is it the player's turn?
    std::function<void(const chess::Move&, bool slide)> play;      // a move; slide: not dropped there by hand
    std::function<void()>                               selectionChanged;  // the board's marks need redrawing

    // While the promotion picker is open, a click belongs to it: a piece
    // finishes the move, anywhere else means "not that move". False if it is
    // not open — the click is someone else's.
    bool promotionClick(const ax::Vec2& point);

    // A press on the board. False if it is not on the board: the scene decides
    // what that means for the selection.
    bool mouseDown(const ax::Vec2& point);
    // True while a press is under way: the screen has to keep up with it.
    bool mouseMove(const ax::Vec2& point);
    void mouseUp(const ax::Vec2& point);

    void select(chess::Square square);
    chess::Square selected() const { return _selected; }

    // Forgets a press, and puts down a piece being carried.
    void cancelPress();
    // Closes the picker; the pawn stays where it was.
    void cancelPromotion();
    // A new position: nothing selected, held or half-promoted.
    void reset();

    bool pressed() const { return _pressed; }

private:
    bool tryMove(chess::Square from, chess::Square to, bool carried);

    BoardNode* _board = nullptr;

    chess::Square _selected = chess::kNoSquare;

    bool          _pressed = false, _dragging = false, _wasSelected = false;
    chess::Square _pressSquare = chess::kNoSquare;
    ax::Vec2      _pressPoint;

    // A pawn is on the last rank and the player is choosing what it becomes.
    chess::Square _promoteFrom = chess::kNoSquare, _promoteTo = chess::kNoSquare;
    bool          _promoteCarried = false;
};
