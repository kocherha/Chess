// The second mode of the game: setting up positions, and solving problems.
//
// It is a scene of its own, pushed over the game and popped to go back, so a
// game in progress waits where it was left — its board as it was, its
// opponent perhaps still thinking — while the player is here. The game's
// watching engine is lent to this scene meanwhile (see GameScene).
//
// Two pages share the board. The editor puts any pieces anywhere and says, as
// each one goes down, whether a game could ever arrive at the position (see
// chess::Setup) and, once it could, what Stockfish finds in it: a forced mate,
// or who is better. Solving takes a position and a goal — mate in so many
// moves, or the best move — and has Stockfish judge every move the player
// makes against it, answering the right ones with the stoutest defence. A
// problem carries no solution of its own; the engine is the arbiter, so any
// move that really does mate in time is accepted.
//
// What solving means — the phases, what the engine is asked and what its
// answers mean — is chess::PuzzleSession, which knows nothing of the screen.
// This scene passes it what the player does and the engine says, and shows
// what it reports.
#pragma once

#include "BoardInput.h"
#include "BoardNode.h"
#include "Button.h"
#include "ConfirmDialog.h"
#include "LookDialog.h"
#include "PieceTray.h"
#include "PuzzleDialog.h"
#include "PuzzlePanel.h"
#include "Puzzles.h"
#include "Wakefulness.h"

#include "chess/Engine.h"
#include "chess/Game.h"
#include "chess/Judge.h"
#include "chess/PuzzleSession.h"
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

    // `engine` is the game's watching engine, lent for as long as this scene
    // is on screen: the game is not looking at anything meanwhile, and a
    // Stockfish of its own each time the problems open would be a process,
    // a network to load and a handshake for nothing. It must outlive the
    // scene. Leaving hands it back clean (onExit).
    static PuzzleScene* create(chess::Engine& engine, Hooks hooks, Opening opening);

    void update(float delta) override;
    void onEnter() override;
    void onExit() override;

private:
    explicit PuzzleScene(chess::Engine& engine) : _engine(engine) {}

    enum class Page { Editor, Solving };

    // What the editor has asked the engine. (Solving asks through the
    // session; see talkToEngine().)
    enum class Question { None, Probe, Shorter };

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
    void takeBack();
    void askHint();
    void showSolution();
    // After anything that may have changed the session: tell the engine what
    // it asks, show what happened on the board, log it, and refresh the page.
    void sessionChanged();
    void refreshSolving();
    std::string noteText(const chess::PuzzleSession::Note& note) const;
    std::string movesText() const;
    bool playerToMove() const { return _page == Page::Solving && _session.playerToMove(); }

    // --- the engine ---
    void ask(Question question, chess::SearchRequest request);  // the editor's questions
    void talkToEngine();                                       // the session's
    void pollEngine();

    // --- input ---
    Dialog* modal() const;
    void onMouseDown(const ax::Vec2& point, bool right);
    void onMouseMove(const ax::Vec2& point);
    void onMouseUp(const ax::Vec2& point);
    void onKeyPressed(ax::EventKeyboard::KeyCode code);
    // The editor's own gestures: a piece taken from a tray or a square and
    // put somewhere, or a click with the brush. (Solving moves pieces the way
    // a game does: _input.)
    void editorDown(const ax::Vec2& point);
    bool editorMove(const ax::Vec2& point);  // true while a press is under way
    void editorUp(const ax::Vec2& point);
    void dropFromTray(const ax::Vec2& point, const PieceTray::Item& item);
    // Forgets a press, and puts down whatever was being carried: the board
    // it belonged to has been shown afresh, or a dialog has taken the mouse.
    void cancelPress();

    // --- the screen ---
    void layout();
    void restyle();
    void refreshMarks();
    void flash(Button* button, const char* text, std::function<void()> restore);
    void flip();
    void openCollection();
    void askBeforeQuitting(bool& closing);
    void keepAwake() { _awake.keepAwake(); }
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

    // Solving: the problem as the collection has it (its id, name, whose it
    // is), and the session that is solving it.
    Puzzle               _puzzle;
    chess::PuzzleSession _session;

    // The engine. The editor's questions carry negative tags, the session's
    // positive ones: one engine, two askers, and each answer goes back to
    // the one that asked.
    chess::Engine& _engine;
    Question      _question = Question::None;
    int           _tag = 0, _nextTag = -1;
    bool _engineWasUsable = false;  // as update() last saw it

    // Input. Solving: as in a game. The editor: a press on a tray or a
    // square, a click or — once the pointer has moved — a drag.
    BoardInput _input;
    struct EditorPress {
        bool            down = false, dragging = false;
        ax::Vec2        point;
        chess::Square   square = chess::kNoSquare;  // a press that began on the board
        PieceTray::Item item;                       // or on a tray
    };
    EditorPress _press;
    ax::Sprite* _carried = nullptr;  // a piece being carried from a tray

    float   _flashFor = 0.0f;
    std::function<void()> _unflash;
    Wakefulness _awake;

    // Nodes.
    ax::DrawNode*  _backdrop = nullptr;
    BoardNode*     _board  = nullptr;
    PieceTray*     _top    = nullptr;
    PieceTray*     _bottom = nullptr;
    ax::Label*     _caption = nullptr;  // under the board while solving: whose move
    PuzzlePanel*   _panel  = nullptr;
    PuzzleDialog*  _collection = nullptr;  // the problems to choose from
    LookDialog*    _looks  = nullptr;
    ConfirmDialog* _confirm = nullptr;
};
