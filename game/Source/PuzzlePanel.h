// The panel beside the board in the problems mode. It has two pages: the
// position editor, and a problem being solved. Like the panel of the game, it
// shows what it is told and hands every click to the scene.
#pragma once

#include "Button.h"
#include "Theme.h"

#include "chess/Types.h"

#include "axmol.h"

#include <array>
#include <functional>
#include <string>
#include <vector>

class PuzzlePanel : public ax::Node
{
public:
    enum class Page { Editor, Solving };
    using Tone = theme::Tone;

    static PuzzlePanel* create();

    void setFrame(const ax::Vec2& origin, const ax::Vec2& size);
    void setPage(Page page);

    // The buttons that are on screen on the page now showing.
    ButtonSet& buttons() { return _live; }

    // The card in the middle: the verdict on a set-up position, or how the
    // solving is going. `detail` may hold several paragraphs, one per line.
    void setStatus(const std::string& headline, const std::string& detail, Tone tone);

    // --- the editor ---
    void setSideToMove(chess::Color colour);
    // One castling right: whether it is ticked, and whether the pieces stand
    // where it could be.
    void setCastling(chess::Color colour, chess::Wing wing, bool wanted, bool possible);
    // The squares a pawn could be taken on en passant, and the one chosen
    // (kNoSquare for none).
    void setEnPassant(const std::vector<chess::Square>& candidates, chess::Square chosen);
    void setFen(const std::string& fen);

    std::function<void(chess::Color)>  onSideToMove;
    std::function<void(chess::Color, chess::Wing)> onCastling;  // toggled
    std::function<void(chess::Square)> onEnPassant;  // kNoSquare for "none"

    // --- solving ---
    void setPuzzle(const std::string& title, const std::string& note, const std::string& goal);
    void setMoves(const std::string& moves);
    // Whether the puzzle is one of the player's own: then it can be deleted,
    // and the button on its row says so.
    void setMine(bool mine);

    // Everywhere.
    Button* looks       = nullptr;
    Button* modeGame    = nullptr;
    Button* modePuzzles = nullptr;
    Button* collection  = nullptr;

    // The editor.
    Button* start    = nullptr;
    Button* clear    = nullptr;
    Button* flip     = nullptr;
    Button* copyFen  = nullptr;
    Button* pasteFen = nullptr;
    Button* solve    = nullptr;
    Button* playFrom = nullptr;
    Button* saveMine = nullptr;

    // Solving.
    Button* next      = nullptr;
    Button* hint      = nullptr;
    Button* solution  = nullptr;
    Button* undo      = nullptr;
    Button* restart   = nullptr;
    Button* edit      = nullptr;
    Button* removeMine = nullptr;
    Button* flipSolving = nullptr;

private:
    bool init() override;

    // The page is laid out from the top down to the card, and the buttons
    // from the bottom up; the card takes what is left between them, and
    // whatever comes under the card (the FEN, the moves) only what remains.
    // Each part takes the edge it starts from and gives back where it ended.
    struct Metrics {
        float width = 0.0f, left = 0.0f;
        bool  compact = false;  // a short window: everything a little tighter
        float button = 0.0f, small = 0.0f, gap = 0.0f;
        float half = 0.0f, third = 0.0f;  // widths of two and of three buttons in a row
    };
    void  layout();
    Metrics metrics() const;
    float layoutHeader(const Metrics& m);
    float layoutEditorButtons(const Metrics& m);
    float layoutSolvingButtons(const Metrics& m);
    float layoutEditorChoices(const Metrics& m, float y);
    float layoutGoal(const Metrics& m, float y);
    float layoutCard(const Metrics& m, float y, float bottom);
    void  fitDetail(float card, float textWidth);
    void  layoutFooter(const Metrics& m, float y, float bottom);
    void collectLive();
    // A label in the panel's own style, anchored at its left edge.
    ax::Label* addLabel(float points, const ax::Color3B& colour);

    Page     _page = Page::Editor;
    ax::Vec2 _size;
    bool     _mine = false;

    ax::DrawNode* _back = nullptr;
    ax::Label *_title = nullptr, *_heading = nullptr;
    ax::Label *_headline = nullptr, *_detail = nullptr;
    std::string _headlineText, _detailText;
    Tone _tone = Tone::Neutral;

    // Editor.
    ax::Label *_toMoveLabel = nullptr, *_castlingLabel = nullptr, *_whiteLabel = nullptr, *_blackLabel = nullptr;
    ax::Label *_passantLabel = nullptr, *_fen = nullptr, *_help = nullptr;
    Button *_white = nullptr, *_black = nullptr;
    std::array<Button*, 4> _castling{};  // white 0-0, white 0-0-0, black 0-0, black 0-0-0
    Button*& castlingButton(chess::Color colour, chess::Wing wing);
    std::array<Button*, 9> _passant{};  // "none" and up to eight squares
    std::array<chess::Square, 9> _passantSquares{};
    std::string _fenText;

    // Solving.
    ax::Label *_note = nullptr, *_goal = nullptr, *_moves = nullptr;
    std::string _noteText, _goalText, _movesText;

    std::vector<ax::Node*> _editorOnly, _solvingOnly;
    ButtonSet _live;
};
