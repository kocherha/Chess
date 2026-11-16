// The pieces the position editor puts on the board: a row of the six of one
// colour beside the board, and an eraser at the end of it.
//
// It is a palette and nothing more. It says what is under a point and lights
// the one that is chosen; what a click or a drag does with it is the scene's
// business.
#pragma once

#include "chess/Types.h"

#include "axmol.h"

#include <array>

class PieceTray : public ax::Node
{
public:
    // A cell of the tray: a piece, or the eraser.
    struct Item {
        bool         eraser = false;
        chess::Piece piece;

        bool none() const { return !eraser && piece.empty(); }
        bool operator==(const Item&) const = default;
    };

    static PieceTray* create(chess::Color colour);

    // The tray is laid out in this rectangle: square cells, as large as the
    // height allows and the width has room for, centred.
    void setFrame(const ax::Vec2& origin, const ax::Vec2& size);

    // The set of pieces has been changed.
    void restyle();

    // What is under a point in world coordinates; none() if nothing.
    Item at(const ax::Vec2& worldPoint) const;

    // The item to light up, or none.
    void setChosen(const Item& item);

private:
    bool initWith(chess::Color colour);
    void rebuild();
    ax::Vec2 cellOrigin(int index) const;

    static constexpr int kCells = 7;  // king, queen, rook, bishop, knight, pawn, eraser
    Item itemAt(int index) const;

    chess::Color _colour = chess::Color::White;
    ax::Vec2 _size;
    float _cell = 0.0f;
    Item  _chosen;

    ax::DrawNode* _back   = nullptr;
    ax::Node*     _pieces = nullptr;
};
