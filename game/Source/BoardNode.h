// The board as a thing on screen: squares, pieces, and the marks drawn over
// them. It shows whatever position it is given and answers one question —
// which square is under this point? — and that is all it knows about chess.
// Whether a move is legal, whose turn it is and what happens next are the
// scene's business.
#pragma once

#include "chess/Judge.h"
#include "chess/Position.h"

#include "axmol.h"

#include <array>
#include <vector>

class BoardNode : public ax::Node
{
public:
    static BoardNode* create();

    // The board is a square of this side with its bottom-left corner at the
    // node's origin.
    void  setSide(float side);
    float side() const { return _side; }
    float squareSize() const { return _side / 8.0f; }

    // Normally white plays up the screen; flipped, black does.
    void setFlipped(bool flipped);
    bool flipped() const { return _flipped; }

    // The look of the pieces or the board has been changed (see Look.h): draw
    // the same position again in the new one.
    void restyle();

    // Puts this position on the board. With `played`, the move that led to it
    // is shown happening: the piece slides, the rook follows when castling,
    // and whatever was captured fades out. `slide` false skips the slide, for
    // a piece the player has just carried there by hand.
    void show(const chess::Position& position, const chess::Move* played = nullptr, bool slide = true);

    // How the game stands for one of the kings, as a badge on his square:
    // "+" in check, "#" mated, a crown for the winner, a white flag for the
    // one who resigned, "½" for a draw of any kind.
    enum class Sign { None, Check, Mate, Won, Resigned, Draw };
    struct KingSign {
        chess::Square square = chess::kNoSquare;
        Sign          sign   = Sign::None;
    };

    struct Marks {
        chess::Square lastFrom = chess::kNoSquare, lastTo = chess::kNoSquare;
        chess::Square selected = chess::kNoSquare;
        chess::Square check    = chess::kNoSquare;  // the king that is in check
        chess::Square hintFrom = chess::kNoSquare, hintTo = chess::kNoSquare;
        // What the last move was worth ("!!", "?", ...): a badge on the corner
        // of the square it went to. None for no badge.
        chess::Mark verdict = chess::Mark::None;
        // The same kind of badge on the kings, for the state of the game. If
        // the last move ended on a king's square too, its own badge moves to
        // the other corner.
        std::array<KingSign, 2> kings{};
        std::vector<chess::Move> targets;           // where the selected piece may go
    };
    void setMarks(const Marks& marks);

    // The square under a point given in world coordinates, or kNoSquare.
    chess::Square squareAt(const ax::Vec2& worldPoint) const;

    // Picking a piece up and carrying it. endDrag() puts it back on its
    // square; if a move was made, the scene calls show() afterwards.
    void beginDrag(chess::Square square, const ax::Vec2& worldPoint);
    void dragTo(const ax::Vec2& worldPoint);
    void endDrag();
    bool dragging() const { return _dragged != nullptr; }

    // A pawn has reached the last rank: offer the four pieces it can become,
    // in a column starting on that square.
    void openPromotion(chess::Square target, chess::Color colour);
    void closePromotion();
    bool promotionOpen() const { return _promotion != nullptr; }
    // The piece under the point, or None if the click missed the column.
    chess::PieceType promotionAt(const ax::Vec2& worldPoint) const;

    // Is anything still moving?
    bool animating() const { return _animations > 0; }

private:
    bool init() override;

    ax::Vec2 origin(chess::Square square) const;  // bottom-left corner, local
    ax::Vec2 centre(chess::Square square) const;
    ax::Sprite* makePiece(chess::Piece piece) const;

    void redrawSquares();
    void redrawMarks();
    void slide(ax::Node* piece, const ax::Vec2& from, const ax::Vec2& to);

    float _side    = 480.0f;
    bool  _flipped = false;

    chess::Position _shown;
    Marks           _marks;

    ax::DrawNode* _squares  = nullptr;
    ax::Node*     _labels   = nullptr;
    ax::DrawNode* _under    = nullptr;  // marks beneath the pieces
    ax::Node*     _pieces   = nullptr;
    ax::DrawNode* _over     = nullptr;  // marks above them
    // What is written on the badges: the last move's and one for each king.
    std::array<ax::Label*, 3> _signs{};
    ax::Node*     _promotion = nullptr;

    std::array<ax::Sprite*, 64> _sprites{};
    ax::Sprite*   _dragged = nullptr;
    chess::Square _dragSquare = chess::kNoSquare;

    chess::Square _promotionTarget = chess::kNoSquare;
    bool          _promotionDown   = true;  // does the column run down the screen?

    int _animations = 0;
};
