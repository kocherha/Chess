// The second mode of the game: setting up positions, and solving problems.
//
// It is a scene of its own, pushed over the game and popped to go back, so a
// game in progress waits where it was left — its engines idle, its board as it
// was — while the player is here.
//
// Two pages share the board. The editor puts any pieces anywhere and says, as
// each one goes down, whether a game could ever arrive at the position (see
// chess::Setup) and, once it could, what Stockfish finds in it: a forced mate,
// or who is better. Solving takes a position and a goal — mate in so many
// moves, or the best move — and has Stockfish judge every move the player
// makes against it, answering the right ones with the stoutest defence. A
// problem carries no solution of its own; the engine is the arbiter, so any
// move that really does mate in time is accepted.
#pragma once

#include "BoardNode.h"
#include "Button.h"
#include "ConfirmDialog.h"
#include "LookDialog.h"
#include "PieceTray.h"
#include "PuzzleDialog.h"
#include "PuzzlePanel.h"
#include "Puzzles.h"

#include "chess/Engine.h"
#include "chess/Game.h"
#include "chess/Judge.h"
#include "chess/Setup.h"

#include "axmol.h"

#include <functional>
#include <optional>
#include <string>

class PuzzleScene : public ax::Scene
{
public:
    // What the game lends this scene: the way back to it, a way to start a
    // game from a position set up here, and whether closing the window would
    // throw a game away.
    struct Hooks {
        std::function<void()>                       backToGame;
        std::function<void(const chess::Position&)> playFrom;
        std::function<bool()>                       gameAtStake;
    };

    // Where to begin: the editor with a position (empty: the starting
    // position), or a problem by id. `screenshot`: as the game's --shot.
    struct Opening {
        std::string fen;
        std::string puzzle;
        std::string screenshot;
    };

    static PuzzleScene* create(Hooks hooks, Opening opening);
    ~PuzzleScene() override;

    void update(float delta) override;
    void onEnter() override;

private:
    enum class Page { Editor, Solving };

    // Where solving stands.
    enum class Phase {
        Preparing,  // the engine is finding the best move first (best-move problems)
        Ready,      // the player's move
        Checking,   // the engine is judging it
        Replying,   // the engine is choosing the defence, or about to play it
        Wrong,      // a wrong move is on the board, about to be taken back
        Solved,
    };

    // What the one engine of this scene has been asked.
    enum class Question { None, Probe, Shorter, Prepare, Check, Recheck, Reply, Hint };

    bool initWith(Hooks hooks, Opening opening);

    // --- the editor ---
    void openEditor(const chess::Setup& setup);
    void edited();  // something on the board or the panel changed
    void place(chess::Square square, chess::Piece piece);
    void probe();
    void refreshEditor();
    std::optional<chess::Position> setupPosition() const;
    void copyFen();
    void pasteFen();
    void saveMine();
    void solveSetup();
    void playFromSetup();

    // --- solving ---
    void openPuzzle(const Puzzle& puzzle);
    void restart();
    void played(const chess::Move& move, bool slide);
    void judged(bool right, const chess::SearchResult& result);
    void reply(const std::string& move);
    void wrong(const std::string& why);
    void takeBack();
    void askHint();
    void showSolution();
    void finish(bool byPlayer);
    void refreshSolving();
    int  attackerMoves() const;  // moves the side solving has made so far
    int  movesLeft() const;
    bool playerToMove() const;
    std::string movesText() const;

    // --- the engine ---
    void ask(Question question, chess::SearchRequest request);
    void pollEngine();
    bool engineUsable() const;

    // --- input ---
    Dialog* modal() const;
    void onMouseDown(const ax::Vec2& point, bool right);
    void onMouseMove(const ax::Vec2& point);
    void onMouseUp(const ax::Vec2& point);
    void onKeyPressed(ax::EventKeyboard::KeyCode code);
    void select(chess::Square square);
    bool tryMove(chess::Square from, chess::Square to, bool carried);
    void dropFromTray(const ax::Vec2& point);

    // --- the screen ---
    void layout();
    void restyle();
    void refreshMarks();
    void flash(Button* button, const char* text, std::function<void()> restore);
    void flip();
    void openCollection();
    void askBeforeQuitting(bool& closing);
    void keepAwake();
    void captureAndQuit();

    Hooks   _hooks;
    Opening _opening;
    Page    _page = Page::Editor;

    // The editor.
    chess::Setup      _setup;
    PieceTray::Item   _brush;     // what a click on the board puts there
    float             _probeIn = -1.0f;  // seconds until the position is given to the engine; < 0: not due
    std::string       _probeFen;  // what the engine said the following about
    bool              _probeDone = false;
    std::optional<int> _probeMate, _probeCp;

    // Solving.
    Puzzle       _puzzle;
    chess::Game  _game;
    chess::Color _attacker = chess::Color::White;
    Phase        _phase    = Phase::Ready;
    bool         _autoSolve = false;  // the engine plays the solving side too: "show the solution"
    bool         _helped    = false;  // a hint or the solution was asked for
    std::string  _bestMove;           // best-move problems: the engine's choice, and its worth
    chess::Score _bestScore;
    std::string  _replyMove;
    float        _replyDelay = 0.0f;
    float        _wrongFor   = 0.0f;  // seconds until a wrong move is taken back
    std::string  _verdictHead, _verdictDetail;
    PuzzlePanel::Tone _verdictTone = PuzzlePanel::Tone::Neutral;
    std::vector<chess::Mark> _marks;  // per move: "!" for the solver's right ones, "?" for the wrong one
    chess::Square _hintFrom = chess::kNoSquare, _hintTo = chess::kNoSquare;

    // The engine.
    chess::Engine _engine;
    Question      _question = Question::None;
    int           _tag = 0, _nextTag = 1;
    std::string   _checkKey;    // the `position` before the move being judged
    std::string   _checkMove;   // that move, UCI
    chess::Engine::State _engineState = chess::Engine::State::Stopped;  // as update() last saw it

    // Input.
    chess::Square _selected = chess::kNoSquare;
    bool          _pressed = false, _dragging = false, _wasSelected = false;
    chess::Square _pressSquare = chess::kNoSquare;
    PieceTray::Item _pressItem;   // a press that began on a tray
    ax::Vec2      _pressPoint;
    ax::Sprite*   _carried = nullptr;  // a piece being carried from a tray
    chess::Square _promoteFrom = chess::kNoSquare, _promoteTo = chess::kNoSquare;
    bool          _promoteCarried = false;

    float   _flashFor = 0.0f;
    std::function<void()> _unflash;
    float   _quiet = 0.0f;
    bool    _quitAsked = false;

    // Nodes.
    ax::DrawNode*  _backdrop = nullptr;
    BoardNode*     _board  = nullptr;
    PieceTray*     _top    = nullptr;
    PieceTray*     _bottom = nullptr;
    ax::Label*     _caption = nullptr;  // under the board while solving: whose move
    PuzzlePanel*   _panel  = nullptr;
    PuzzleDialog*  _list   = nullptr;
    LookDialog*    _looks  = nullptr;
    ConfirmDialog* _confirm = nullptr;
};
