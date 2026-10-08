// What stands around the board: the strip with a player's name above and below
// it, and the panel on the right with the state of the game, the moves and the
// buttons. Both are told what to show; neither knows where it came from.
#pragma once

#include "Button.h"

#include "chess/Judge.h"
#include "chess/Types.h"

#include "axmol.h"

#include <functional>
#include <string>
#include <vector>

// One player: who they are, what they have captured, and how far ahead that
// puts them.
class PlayerBar : public ax::Node
{
public:
    static PlayerBar* create();

    void setFrame(const ax::Vec2& origin, const ax::Vec2& size);

    // `trophies` are the opponent's pieces this player has taken; `lead` is
    // text such as "+3" (empty when not ahead); `toMove` lights the marker.
    void set(const std::string& name, chess::Color colour, const std::vector<chess::PieceType>& trophies,
             const std::string& lead, bool toMove);

    // The set of pieces has been changed: the captured ones are drawn in it too.
    void restyle();

private:
    bool init() override;
    void rebuild();

    ax::Vec2 _size;
    std::string _name, _lead;
    chess::Color _colour = chess::Color::White;
    std::vector<chess::PieceType> _trophies;
    bool _toMove = false;

    ax::DrawNode* _marker = nullptr;
    ax::Node*     _content = nullptr;
};

class SidePanel : public ax::Node
{
public:
    enum class Tone { Neutral, Good, Warning };

    static SidePanel* create();

    void setFrame(const ax::Vec2& origin, const ax::Vec2& size);

    // The line under the title: the level, or that two people are playing.
    void setSubtitle(const std::string& text);
    void setStatus(const std::string& headline, const std::string& detail, Tone tone = Tone::Neutral);

    // The moves so far, already in the notation to show. `whiteFirst` is false
    // when the game began with black to move (a position set up from a FEN).
    void setMoves(const std::vector<std::string>& moves, bool whiteFirst = true);

    // What each of those moves was worth: "!!", "?" and the rest, written
    // after the move in its own colour. One per move; they arrive later than
    // the moves do and change nothing about where the list is scrolled to.
    void setMarks(const std::vector<chess::Mark>& marks);

    // How the game ended ("1–0"), on a line of its own under the last move.
    // Empty while it is still being played.
    void setResult(const std::string& result);

    // Scrolls the move list by this many rows (positive: towards the start).
    // Fractions add up: a trackpad sends its movement as many small steps, and
    // the list moves a row whenever they have come to one. True if the list
    // moved.
    bool scroll(float rows);
    bool contains(const ax::Vec2& worldPoint) const;
    // Is this point over the move list (the only place the wheel means
    // anything)?
    bool overList(const ax::Vec2& worldPoint) const;

    // The mouse over the move list. A press on the scroll bar takes hold of
    // it — on the thumb where it stands, anywhere else in the track by
    // bringing the thumb there first — and a press and release on one move is
    // a click on that move. mouseDown() says whether the press was the list's;
    // mouseMove() whether anything on screen changed because of it.
    bool mouseDown(const ax::Vec2& worldPoint);
    bool mouseMove(const ax::Vec2& worldPoint);
    void mouseUp(const ax::Vec2& worldPoint);
    bool dragging() const { return _draggingThumb; }

    // A move was clicked: its index in the list given to setMoves().
    std::function<void(int index)> onMoveClicked;

    // Draws a ring round one move for as long as it is set — the answer to
    // "which one did I just copy?". -1 for none.
    void markMove(int index);

    Button* looks   = nullptr;  // pieces and board: what they look like
    Button* copy    = nullptr;  // the whole game to the clipboard
    Button* newGame = nullptr;
    Button* undo    = nullptr;
    Button* hint    = nullptr;
    Button* flip    = nullptr;
    Button* resign  = nullptr;

private:
    bool init() override;
    void layout();
    void rebuildMoves();
    int  rowCount() const;
    int  visibleRows() const;

    // The scroll bar's thumb as it stands now, in this node's coordinates.
    struct Thumb {
        bool  shown  = false;  // is there more than fits?
        float top    = 0.0f;   // where the track begins
        float travel = 0.0f;   // how far the thumb can move along it
        float length = 0.0f;
        float at     = 0.0f;   // its top edge
        int   last   = 0;      // the first row when scrolled to the end
    };
    Thumb thumb() const;
    bool  overBar(const ax::Vec2& local) const;
    void  dragThumbTo(float y);
    // The move drawn under this point, or -1.
    int   moveAt(const ax::Vec2& local) const;

    ax::Vec2 _size;
    std::vector<std::string> _moves;
    std::vector<chess::Mark> _marks;
    std::string _result;
    bool _whiteFirst = true;
    int  _firstRow   = 0;     // first row on screen
    bool _following  = true;  // stay at the end as moves arrive
    float _scrollCarry = 0.0f;  // the part of a row scroll() has not used yet

    int  _hoverMove   = -1;  // the move under the pointer
    int  _pressedMove = -1;  // the move the button went down on
    int  _markedMove  = -1;
    bool _barHot        = false;  // the pointer is on the scroll bar
    bool _draggingThumb = false;
    float _thumbGrab    = 0.0f;   // from the thumb's top edge to where it is held

    // Where the move list sits, computed by layout().
    float _listTop = 0.0f, _listBottom = 0.0f;

    ax::DrawNode* _back     = nullptr;
    ax::Label*    _title    = nullptr;
    ax::Label*    _subtitle = nullptr;
    ax::Label*    _headline = nullptr;
    ax::Label*    _detail   = nullptr;
    ax::Label*    _movesTitle = nullptr;
    ax::Node*     _moveRows = nullptr;
    Tone          _tone = Tone::Neutral;
    std::string   _headlineText, _detailText;
};
