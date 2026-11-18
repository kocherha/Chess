// The game itself: one board, one opponent, and everything that happens
// between a click and a move.
//
// Three things meet here and nowhere else. The rules (chess::Game) say what is
// legal and when it is over. Stockfish (chess::Engine) is asked for a move
// when it is the computer's turn and answers in its own time. And the nodes —
// the board, the bars, the panel, the dialogs — show whatever they are told.
// The scene is the only part that knows about all three.
//
// Stockfish runs twice. One copy is the opponent, held back to the level the
// player chose. The other only watches: it looks at every position at full
// strength and says who is better, which is what the bar beside the board
// shows, where a hint comes from, and what the marks on the moves ("!", "??")
// are worked out from. Keeping them apart means the opponent's handicap never
// leaks into the evaluation, and asking for a hint never changes how the
// opponent plays.
#pragma once

#include "BoardInput.h"
#include "BoardNode.h"
#include "Button.h"
#include "ConfirmDialog.h"
#include "EvalBar.h"
#include "LookDialog.h"
#include "NewGameDialog.h"
#include "Panels.h"
#include "Wakefulness.h"

#include "chess/Engine.h"
#include "chess/Game.h"
#include "chess/Judge.h"
#include "chess/Opponent.h"
#include "chess/Watcher.h"

#include "axmol.h"

#include <map>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <vector>

class GameScene : public ax::Scene
{
public:
    // What the command line asked for. Mostly for looking at the game without
    // a person at the mouse: a build machine, a bug report, a screenshot.
    struct Launch {
        std::string screenshot;          // --shot <file>: let the screen settle, save a PNG, quit
        std::string fen;                 // --fen "<fen>": start from this position
        std::vector<std::string> moves;  // --moves "e2e4 e7e5 ...": then play these
        // --level N, --elo R, --side white|black|random, --two-players. Each
        // is empty unless given, in which case it overrides what was
        // remembered from the last game. --elo is a strength between the
        // levels: the nearest the scale has to that rating.
        std::optional<int>                 level, elo;
        std::optional<GameSetup::Side>     side;
        std::optional<GameSetup::Opponent> opponent;
        // --pieces <id>, --board <id>: how they look, for this run only. The
        // ids are the ones in Look.cpp ("classic", "gothic", "wood", ...).
        std::string pieces, board;
        bool        skipDialog = false;  // --play: straight to the board
        // --autoplay N: the person's moves are made at random for N games,
        // then the program exits. A soak test — every kind of move, capture,
        // promotion and ending goes through the same code a click does.
        int         autoplayGames = 0;
        // The problems mode, straight away: --puzzles (the editor), --edit
        // "<fen>" (the editor with this position), --puzzle <id> (one from
        // the collection, to solve).
        bool        puzzles = false;
        std::string editFen;
        std::string puzzle;
    };
    static Launch& launch();

    ~GameScene() override;

    bool init() override;
    void update(float delta) override;
    // Back from the problems mode: the window may have changed size and the
    // pieces their look while this scene was not on screen.
    void onEnter() override;

private:
    // --- the flow of a game ---
    void startGame(const GameSetup& setup);
    void play(const chess::Move& move, bool slide);
    // The position on the board is about to change, or has: what was asked
    // about the old one — the opponent's reply, a hint — is withdrawn, and
    // what was drawn for it — a selection, a piece in hand, the hint's arrow —
    // is wiped.
    void withdrawQuestions();
    void forgetPositionMarks();
    void askOpponentIfItsTurn();
    void sendToOpponent();  // what chess::Opponent wants of its engine
    void pollOpponent();
    void playRandomMove();

    // The watching engine: start it on the position now on the board, and
    // collect what it has to say.
    void analyse();
    void pollAnalysis();
    void sendToWatcher();    // what chess::Watcher wants of its engine
    void takeFromWatcher();  // and what it has for the screen
    void showHint(const std::string& uci);

    // What it has said about the game's positions, and the marks that follow,
    // are _review's (see chess/GameReview.h). The scene runs the searches:
    // the position on the board first, then — with nothing more urgent — the
    // ones the game moved on from too quickly.
    void marksChanged();
    void showMoves();

    bool againstComputer() const { return _setup.opponent == GameSetup::Opponent::Computer; }
    bool humanToMove() const;
    // The engine that watches starts with the scene. The opponent starts
    // only when a game against the computer needs it: two people at one
    // board never wake it. Each does nothing if its engine is running.
    void startAnalysis();
    void startOpponent();
    // The UCI_Elo range Stockfish declares. Both processes run the same
    // program, so whichever has said it already will do.
    std::optional<chess::Engine::Spin> eloRange() const;

    // The choices in the new-game dialog are remembered between runs.
    static GameSetup rememberedSetup();
    static void remember(const GameSetup& setup);
    // The rating the opponent is really asked for at this strength.
    int opponentElo(const chess::Level& level) const;

    // --- what the player can do ---
    // A game with moves on the board and no result yet: something to lose.
    bool gameAtStake() const;
    void openNewGame();      // asks first if there is
    void showNewGameDialog();
    void askBeforeQuitting(bool& closing);
    void openLooks();
    // The other mode: setting up positions and solving problems. The game
    // waits underneath it as it was.
    void openPuzzles();
    // A position from the editor to play a game from: the new-game dialog,
    // after asking if a game would be lost.
    void offerGameFrom(const chess::Position& position);
    void restyle();
    // To the clipboard: the whole game as PGN, or one move as it is shown.
    void copyGame();
    void copyMove(int index);
    void showCopied();
    void undo();
    void flip();
    void askHint();
    void resign();

    // --- input ---
    // The dialog that is open, if one is. While it is, it gets everything.
    Dialog* modal() const;
    void onMouseDown(const ax::Vec2& point);
    void onMouseMove(const ax::Vec2& point);
    void onMouseUp(const ax::Vec2& point);
    void onKeyPressed(ax::EventKeyboard::KeyCode code);

    // --- putting the state on screen ---
    void layout();
    void refresh();
    void refreshMarks();
    void refreshStatus();
    void refreshBars();

    // Draws for a moment, then stops the render loop until something happens.
    void keepAwake() { _awake.keepAwake(); }
    void captureAndQuit();

    // --- state ---
    chess::Game   _game;
    // The computer as the other player: when and how it answers (see
    // chess::Opponent), and the Stockfish it asks, started for the first game
    // against it.
    chess::Opponent _opponent;
    chess::Engine   _opponentEngine;
    GameSetup     _setup;
    chess::Color  _human = chess::Color::White;  // the person's side against the computer
    bool          _started = false;              // has the first game been set up?
    std::optional<chess::Position> _startFrom;   // the next new game begins here, not at the start
    bool _lentAnalysis = false;  // the watching engine is the problems mode's for now

    // The engine that watches the game — the bar, the marks, the hint (see
    // chess::Watcher) — and the Stockfish it asks, which the problems borrow.
    chess::Watcher _watcher;
    chess::Engine  _watcherEngine;
    chess::Engine::State _opponentState = chess::Engine::State::Stopped;  // as last seen by update()

    // The person's colour when they asked for a random one, and their moves
    // under --autoplay.
    std::mt19937 _rng{std::random_device{}()};

    chess::Square _hintFrom = chess::kNoSquare, _hintTo = chess::kNoSquare;
    std::string   _hintText;

    // Selecting, clicking and dragging pieces, and choosing a promotion.
    BoardInput _input;

    // "Copied" stays on the button for this long after something was.
    float _copiedFor = 0.0f;

    Wakefulness _awake;

    // --- nodes ---
    ax::DrawNode*  _backdrop = nullptr;
    BoardNode*     _board  = nullptr;
    EvalBar*       _evalBar = nullptr;
    PlayerBar*     _top    = nullptr;
    PlayerBar*     _bottom = nullptr;
    SidePanel*     _panel  = nullptr;
    NewGameDialog* _dialog  = nullptr;
    LookDialog*    _looks   = nullptr;
    ConfirmDialog* _confirm = nullptr;
    ButtonSet      _buttons;
};
