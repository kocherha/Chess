// What stands around the board: the strip with a player's name above and below
// it, and the panel on the right with the state of the game, the moves and the
// buttons. Both are told what to show; neither knows where it came from.
#pragma once

#include "Button.h"

#include "chess/Types.h"

#include "axmol.h"

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

    // Scrolls the move list by this many rows (positive: towards the start).
    void scroll(int rows);
    bool contains(const ax::Vec2& worldPoint) const;

    Button* looks   = nullptr;  // pieces and board: what they look like
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

    ax::Vec2 _size;
    std::vector<std::string> _moves;
    bool _whiteFirst = true;
    int  _firstRow   = 0;     // first row on screen
    bool _following  = true;  // stay at the end as moves arrive

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
