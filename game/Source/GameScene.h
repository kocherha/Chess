// The game itself: one board, one opponent, and everything that happens
// between a click and a move.
//
// Three things meet here and nowhere else. The rules (chess::Game) say what is
// legal and when it is over. Stockfish (chess::Engine) is asked for a move
// when it is the computer's turn and answers in its own time. And the nodes —
// the board, the bars, the panel, the dialog — show whatever they are told.
// The scene is the only part that knows about all three.
//
// Stockfish runs twice. One copy is the opponent, held back to the level the
// player chose. The other only watches: it looks at every position at full
// strength and says who is better, which is what the bar beside the board
// shows and where a hint comes from. Keeping them apart means the opponent's
// handicap never leaks into the evaluation, and asking for a hint never
// changes how the opponent plays.
#pragma once

#include "BoardNode.h"
#include "Button.h"
#include "EvalBar.h"
#include "LookDialog.h"
#include "NewGameDialog.h"
#include "Panels.h"

#include "chess/Engine.h"
#include "chess/Game.h"

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
        std::string screenshot;          // --shot <file>: draw, save a PNG, quit
        std::string fen;                 // --fen "<fen>": start from this position
        std::vector<std::string> moves;  // --moves "e2e4 e7e5 ...": then play these
        // --level N, --side white|black|random, --two-players. Each is empty
        // unless given, in which case it overrides what was remembered from
        // the last game.
        std::optional<int>                 level;
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
    };
    static Launch& launch();

    ~GameScene() override;

    bool init() override;
    void update(float delta) override;

private:
    // --- the flow of a game ---
    void startGame(const GameSetup& setup);
    void play(const chess::Move& move, bool slide);
    void askEngineIfItsTurn();
    void pollEngine();
    void playRandomMove();
    void playPendingReply();

    // The watching engine: start it on the position now on the board, and
    // collect what it has to say.
    void analyse();
    void pollAnalysis();
    void showScore(const chess::SearchResult& result);
    void showHint();
    bool analysisUsable() const;

    bool againstComputer() const { return _setup.opponent == GameSetup::Opponent::Computer; }
    bool humanToMove() const;
    bool engineUsable() const;
    void startEngine();

    // The choices in the new-game dialog are remembered between runs.
    static GameSetup rememberedSetup();
    static void remember(const GameSetup& setup);

    // --- what the player can do ---
    void openNewGame();
    void openLooks();
    void restyle();
    void undo();
    void flip();
    void askHint();
    void resign();
    void select(chess::Square square);
    bool tryMove(chess::Square from, chess::Square to, bool carried);
    void finishPromotion(chess::PieceType piece);
    void cancelPromotion();

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
    void onWindowResized();

    // Draws for a moment, then stops the render loop until something happens.
    void keepAwake();
    void captureAndQuit();

    // --- state ---
    chess::Game   _game;
    chess::Engine _engine;
    GameSetup     _setup;
    chess::Color  _human = chess::Color::White;  // the person's side against the computer
    bool          _started = false;              // has the first game been set up?

    // Requests to an engine carry a tag; an answer whose tag is no longer the
    // one we are waiting for is an answer to a question nobody is asking any
    // more (the move was taken back, a new game began).
    int  _nextTag = 1;
    int  _moveTag = 0;
    bool _engineLost = false;

    // The watching engine, and what it has found out about the position on
    // the board: nothing yet, a score still being refined, or its final word
    // together with the move it would play.
    chess::Engine  _analysis;
    int            _analysisTag  = 0;
    bool           _analysisDone = false;
    std::string    _analysisBest;        // UCI; set when the analysis has finished
    bool           _hintWanted   = false;  // show that move as soon as it is known
    bool           _barShown     = false;  // is there a bar in the layout right now?
    chess::Engine::State _engineState = chess::Engine::State::Stopped;  // as last seen by update()

    // A reply that does not come from a search: played after a short pause,
    // as if thought about. Either a move the opponent has already made in this
    // very position (see _replies), or — below the engine's weakest setting —
    // one picked at random, in which case _replyMove is empty until then.
    bool        _replyPending = false;
    float       _replyDelay   = 0.0f;
    std::string _replyMove;
    std::mt19937 _rng{std::random_device{}()};

    // What the opponent answered in each position it has been asked about in
    // this game, by the moves that led there. A weakened engine chooses with
    // a roll of the dice; without this, taking a move back and playing it
    // again would get a different answer each time — sometimes a far worse
    // one — and "take back" would double as "roll again".
    std::map<std::string, std::string> _replies;
    std::string _moveKey;  // the position the search in progress was asked about

    chess::Square _selected = chess::kNoSquare;
    chess::Square _hintFrom = chess::kNoSquare, _hintTo = chess::kNoSquare;
    std::string   _hintText;

    // A press on a piece is either a click (select it) or the start of a drag
    // (carry it); which one is only known once the pointer has moved.
    bool          _pressed = false, _dragging = false, _wasSelected = false;
    chess::Square _pressSquare = chess::kNoSquare;
    ax::Vec2      _pressPoint;

    // A pawn is on the last rank and the player is choosing what it becomes.
    chess::Square _promoteFrom = chess::kNoSquare, _promoteTo = chess::kNoSquare;
    bool          _promoteCarried = false;

    float       _quiet        = 0.0f;     // seconds since anything last happened
    const char* _awakeBecause = nullptr;  // what update() last found to wait for, if anything

    // --- nodes ---
    ax::DrawNode*  _backdrop = nullptr;
    BoardNode*     _board  = nullptr;
    EvalBar*       _evalBar = nullptr;
    PlayerBar*     _top    = nullptr;
    PlayerBar*     _bottom = nullptr;
    SidePanel*     _panel  = nullptr;
    NewGameDialog* _dialog = nullptr;
    LookDialog*    _looks  = nullptr;
    ButtonSet      _buttons;
};
