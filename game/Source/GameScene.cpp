#include "GameScene.h"

#include "Desktop.h"
#include "EventLog.h"
#include "Look.h"
#include "PuzzleScene.h"
#include "Shapes.h"
#include "Strings.h"
#include "Theme.h"

#include "chess/Difficulty.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <thread>

using namespace ax;
using namespace chess;

// The path the build found Stockfish at, when it found one. See
// chess::findEngine for where else it is looked for.
#ifndef CHESS_ENGINE_FALLBACK
#    define CHESS_ENGINE_FALLBACK ""
#endif

namespace {

constexpr float kMargin    = 26.0f;
constexpr float kBarHeight = 38.0f;

// The evaluation bar and the space between it and the board.
constexpr float kEvalWidth = 24.0f;
constexpr float kEvalGap   = 12.0f;

// How long the watching engine looks at a position, at most. It reports as it
// goes, so the bar starts moving within a few hundredths of a second; this is
// only where it stops refining.
constexpr int kAnalysisMs    = 1000;
constexpr int kAnalysisDepth = 22;
// Its first few iterations are guesses that swing wildly. Nothing shallower
// than this is shown unless it is all there will ever be.
constexpr int kAnalysisMinDepth = 6;

// The pause before a reply that needed no thinking.
constexpr float kReplyPause = 0.45f;

// How long "copied" stays up after something has been put on the clipboard.
constexpr float kCopiedSeconds = 1.6f;

// How many rows of the move list one unit of scrolling is worth. The unit is
// whatever the system reports, and the systems do not agree. A wheel on Linux
// or Windows sends 1 for each notch, and a notch should move a couple of
// rows. macOS sends a stream of small numbers — from a trackpad, a tenth of
// each point the fingers travel, and then more as the gesture coasts — so
// there a unit is ten points of movement, which is two fifths of a row: the
// list keeps pace with the fingers, as it would in any other window.
#if (AX_TARGET_PLATFORM == AX_PLATFORM_MAC)
constexpr float kRowsPerScrollUnit = 0.4f;
#else
constexpr float kRowsPerScrollUnit = 2.0f;
#endif

unsigned hardwareThreads() {
    return std::thread::hardware_concurrency();
}

}  // namespace

GameScene::Launch& GameScene::launch() {
    static Launch options;
    return options;
}

GameScene::~GameScene() {
    // Says goodbye to Stockfish and waits for it to leave, so that closing the
    // window never leaves an engine running behind it.
    _engine.shutdown();
    _analysis.shutdown();
}

// --- setting up --------------------------------------------------------------------

bool GameScene::init() {
    if (!Scene::init())
        return false;

    // What the pieces and the board look like has to be known before either
    // is drawn: what was chosen last time, unless the command line says
    // otherwise for this run.
    look::restore();
    if (const std::string& id = launch().pieces; !id.empty() && !look::choosePieces(id, false))
        AXLOGW("--pieces ignored: there is no set called {}", id);
    if (const std::string& id = launch().board; !id.empty() && !look::chooseBoard(id, false))
        AXLOGW("--board ignored: there is no board called {}", id);

    _backdrop = DrawNode::create();
    addChild(_backdrop, -1);

    _board   = BoardNode::create();
    _input.attach(_board);
    _input.position         = [this]() -> const Position& { return _game.position(); };
    _input.mayMove          = [this] { return humanToMove(); };
    _input.play             = [this](const Move& move, bool slide) { play(move, slide); };
    _input.selectionChanged = [this] { refreshMarks(); };
    _evalBar = EvalBar::create();
    _top    = PlayerBar::create();
    _bottom = PlayerBar::create();
    _panel  = SidePanel::create();
    _dialog = NewGameDialog::create();
    _looks   = LookDialog::create();
    _confirm = ConfirmDialog::create();
    addChild(_board, 1);
    addChild(_evalBar, 1);
    addChild(_top, 1);
    addChild(_bottom, 1);
    addChild(_panel, 2);
    addChild(_dialog, 10);
    addChild(_looks, 10);
    addChild(_confirm, 11);  // it can be asked over either of the others

    _panel->looks->onClick   = [this] { openLooks(); };
    _panel->copy->onClick    = [this] { copyGame(); };
    _panel->onMoveClicked    = [this](int index) { copyMove(index); };
    _panel->newGame->onClick = [this] { openNewGame(); };
    _panel->undo->onClick    = [this] { undo(); };
    _panel->hint->onClick    = [this] { askHint(); };
    _panel->flip->onClick    = [this] { flip(); };
    _panel->resign->onClick  = [this] { resign(); };
    _panel->modePuzzles->onClick = [this] { openPuzzles(); };
    for (Button* button : {_panel->modeGame, _panel->modePuzzles, _panel->looks, _panel->copy, _panel->newGame,
                           _panel->undo, _panel->hint, _panel->flip, _panel->resign})
        _buttons.add(button);

    _looks->onChange = [this] { restyle(); };
    _looks->onDone   = [this] { _looks->close(); };

    _dialog->onPlay = [this](const GameSetup& setup) {
        _dialog->close();
        remember(setup);
        startGame(setup);
    };
    _dialog->onPuzzles = [this] {
        // The dialog stays as it is underneath: back from the problems, the
        // choice of a game is still to be made.
        openPuzzles();
    };
    _dialog->onCancel = [this] {
        if (_started) {
            _dialog->close();
            _startFrom.reset();  // that position was only for the game not begun
        }
    };

    // One mouse listener for the whole scene. Who a click belongs to — an
    // open dialog, a button, the promotion column, the board — is decided
    // here, in that order, instead of by whichever node happened to hear it
    // first.
    auto* mouse        = EventListenerMouse::create();
    mouse->onMouseDown = [this](EventMouse* event) {
        if (event->getMouseButton() == EventMouse::MouseButton::BUTTON_LEFT)
            onMouseDown(event->getLocation());
        return true;
    };
    mouse->onMouseUp = [this](EventMouse* event) {
        if (event->getMouseButton() == EventMouse::MouseButton::BUTTON_LEFT)
            onMouseUp(event->getLocation());
        return true;
    };
    mouse->onMouseMove = [this](EventMouse* event) {
        onMouseMove(event->getLocation());
        return true;
    };
    mouse->onMouseScroll = [this](EventMouse* event) {
        // The wheel belongs to the move list, and only while the pointer is
        // over it. Anywhere else there is nothing to scroll.
        if (modal() != nullptr || !_panel->overList(event->getLocation()))
            return true;
        // Wheel up is negative here; up means "earlier moves".
        if (_panel->scroll(-event->getScrollY() * kRowsPerScrollUnit))
            keepAwake();
        return true;
    };
    _eventDispatcher->addEventListenerWithSceneGraphPriority(mouse, this);

    auto* keyboard         = EventListenerKeyboard::create();
    keyboard->onKeyPressed = [this](EventKeyboard::KeyCode code, Event*) { onKeyPressed(code); };
    _eventDispatcher->addEventListenerWithSceneGraphPriority(keyboard, this);

#if defined(AX_PLATFORM_PC)
    // The system's half of "something happened". Resized, moved to another
    // display, brought back to the front: all three can leave stale pixels on
    // a window that has stopped drawing, so each of them buys a few frames.
    //
    // Scene-graph priority, not addCustomEventListener: the latter registers
    // with the dispatcher itself and would outlive this scene.
    const auto whenWindow = [this](std::string_view event, std::function<void(EventCustom*)> what) {
        _eventDispatcher->addEventListenerWithSceneGraphPriority(
            EventListenerCustom::create(event, std::move(what)), this);
    };
    whenWindow(RenderViewImpl::EVENT_WINDOW_POSITIONED, [this](EventCustom*) { keepAwake(); });
    whenWindow(RenderViewImpl::EVENT_WINDOW_FOCUSED, [this](EventCustom*) { keepAwake(); });
    whenWindow(RenderViewImpl::EVENT_WINDOW_RESIZED, [this](EventCustom*) { onWindowResized(); });
    // The close button, Cmd+Q, the window manager: all of them arrive here
    // carrying a flag that says "yes, close", which a listener may turn off.
    whenWindow(RenderViewImpl::EVENT_WINDOW_CLOSE, [this](EventCustom* event) {
        if (auto* closing = static_cast<bool*>(event->getUserData()))
            askBeforeQuitting(*closing);
    });
#endif

    startEngine();

    _board->show(_game.position());
    layout();
    refresh();

    // Start from what was chosen last time; the command line has the last word.
    const Launch& options = launch();
    _setup                = rememberedSetup();
    if (options.level)
        _setup.strength = strengthOfLevel(*options.level);
    if (options.elo)
        _setup.strength = strengthForElo(*options.elo);
    if (options.side)
        _setup.side = *options.side;
    if (options.opponent)
        _setup.opponent = *options.opponent;

    const bool puzzlesFirst = options.puzzles || !options.editFen.empty() || !options.puzzle.empty();
    if (options.skipDialog || !options.fen.empty() || !options.moves.empty() || options.autoplayGames > 0)
        startGame(_setup);
    else
        openNewGame();

    scheduleUpdate();
    keepAwake();

    if (puzzlesFirst) {
        // Once this scene is running: a scene can only be pushed over one.
        scheduleOnce([this](float) { openPuzzles(); }, 0.0f, "puzzles");
    } else if (!options.screenshot.empty()) {
        captureAndQuit();
    }
    return true;
}

void GameScene::onEnter() {
    Scene::onEnter();
    if (_board == nullptr)
        return;
    layout();
    restyle();
    refresh();
    keepAwake();
}

void GameScene::openPuzzles() {
    _input.cancelPromotion();
    _input.select(kNoSquare);

    PuzzleScene::Hooks hooks;
    hooks.backToGame  = [] { Director::getInstance()->popScene(); };
    hooks.playFrom    = [this](const Position& position) {
        Director::getInstance()->popScene();
        offerGameFrom(position);
    };
    hooks.gameAtStake = [this] { return gameAtStake(); };

    // What the command line asked for is for the first visit only.
    Launch& options = launch();
    PuzzleScene::Opening opening;
    opening.fen    = options.editFen;
    opening.puzzle = options.puzzle;
    if (options.puzzles || !options.editFen.empty() || !options.puzzle.empty())
        opening.screenshot = options.screenshot;
    options.puzzles = false;
    options.editFen.clear();
    options.puzzle.clear();

    if (auto* scene = PuzzleScene::create(std::move(hooks), std::move(opening)))
        Director::getInstance()->pushScene(scene);
    keepAwake();
}

void GameScene::offerGameFrom(const Position& position) {
    _startFrom = position;
    if (!gameAtStake()) {
        showNewGameDialog();
        return;
    }
    _quitAsked = false;
    _confirm->open(text::kUnfinishedTitle, text::kLoseByNewGame, text::kNewGame, [this] { showNewGameDialog(); },
                   [this] { _startFrom.reset(); });
    keepAwake();
}

void GameScene::startEngine() {
    const std::string path = findEngine(CHESS_ENGINE_FALLBACK);
    if (path.empty()) {
        AXLOGW("no Stockfish found: only the two-player game is available");
        return;
    }

    // CHESS_UCI_LOG=1 prints both conversations: "uci" is the opponent,
    // "eval" the engine that watches.
    if (std::getenv("CHESS_UCI_LOG") != nullptr) {
        _engine.setTrace([](char direction, const std::string& line) {
            std::fprintf(stderr, "uci  %c %s\n", direction, line.c_str());
        });
        _analysis.setTrace([](char direction, const std::string& line) {
            std::fprintf(stderr, "eval %c %s\n", direction, line.c_str());
        });
    }

    // Two processes from one program: whichever of them is not running.
    if (!_engine.usable()) {
        if (_engine.start(path))
            AXLOGI("engine: {}", path);
        else
            AXLOGW("engine {} did not start: {}", path, _engine.error());
    }
    if (!_analysis.usable() && !_analysis.start(path))
        AXLOGW("analysis engine did not start: {}", _analysis.error());
}

// The rating the opponent is really asked for at this strength: the one on
// the scale, brought inside the range this engine says it has.
int GameScene::opponentElo(const Level& level) const {
    const auto range = _engine.spin("UCI_Elo");
    if (!range || range->min <= 0 || range->max < range->min)
        return level.elo;
    return std::clamp(level.elo, range->min, range->max);
}

GameSetup GameScene::rememberedSetup() {
    auto* store = UserDefault::getInstance();
    GameSetup setup;
    // The place on the scale, or — from a version that knew only the eight
    // levels — the level, which is a place on the scale too.
    const int level = store->getIntegerForKey("level", defaultLevelNumber());
    setup.strength  = std::clamp(store->getIntegerForKey("strength", strengthOfLevel(level)), 0, strengthCount() - 1);
    setup.opponent = store->getIntegerForKey("opponent", 0) == 1 ? GameSetup::Opponent::Human
                                                                 : GameSetup::Opponent::Computer;
    switch (store->getIntegerForKey("side", 0)) {
    case 1:  setup.side = GameSetup::Side::Black; break;
    case 2:  setup.side = GameSetup::Side::Random; break;
    default: setup.side = GameSetup::Side::White; break;
    }
    return setup;
}

void GameScene::remember(const GameSetup& setup) {
    auto* store = UserDefault::getInstance();
    store->setIntegerForKey("strength", setup.strength);
    // The level it has reached, for a version that knows only levels.
    store->setIntegerForKey("level", levelAtStrength(setup.strength).number);
    store->setIntegerForKey("opponent", setup.opponent == GameSetup::Opponent::Human ? 1 : 0);
    store->setIntegerForKey("side", setup.side == GameSetup::Side::Black    ? 1
                                    : setup.side == GameSetup::Side::Random ? 2
                                                                            : 0);
    store->flush();
}

// --- the flow of a game ------------------------------------------------------------

void GameScene::startGame(const GameSetup& setup) {
    _setup   = setup;
    _started = true;

    // A game against the computer needs the computer. If the engine has died
    // since the last game, this is the moment to bring it back.
    if (againstComputer() && !_engine.usable())
        startEngine();
    if (againstComputer() && !_engine.usable())
        _setup.opponent = GameSetup::Opponent::Human;

    switch (_setup.side) {
    case GameSetup::Side::White:  _human = Color::White; break;
    case GameSetup::Side::Black:  _human = Color::Black; break;
    case GameSetup::Side::Random: _human = (_rng() & 1u) != 0 ? Color::White : Color::Black; break;
    }

    if (!_analysis.usable())
        startEngine();

    _engine.cancel();
    _engine.newGame();
    _analysis.newGame();
    _moveTag      = 0;
    _engineLost   = false;
    _replyPending = false;
    _hintWanted   = false;
    _replies.clear();
    _review.clear();
    _searchKey.clear();

    // The command line may have asked for a particular start. That is honoured
    // once, for the first game; "new game" after it means a new game.
    Launch& options = launch();
    _game           = Game();
    if (_startFrom) {
        _game = Game(*_startFrom);
        _startFrom.reset();
    }
    if (!options.fen.empty()) {
        std::string error;
        if (const auto position = Position::fromFen(options.fen, &error))
            _game = Game(*position);
        else
            AXLOGW("--fen ignored: {}", error);
        options.fen.clear();
    }
    for (const std::string& move : options.moves) {
        if (!_game.playUci(move) && !_game.playSan(move)) {
            AXLOGW("--moves stopped at {}: not legal here", move);
            break;
        }
    }
    options.moves.clear();

    _input.reset();
    _hintFrom = _hintTo = kNoSquare;
    _hintText.clear();
    _board->closePromotion();

    // The player sits at the bottom of their own side of the board.
    _board->setFlipped(againstComputer() && _human == Color::Black);
    _evalBar->setFlipped(_board->flipped());
    _board->show(_game.position());
    eventlog::note("game " + _game.position().fen());

    refresh();
    analyse();
    askEngineIfItsTurn();
    keepAwake();
}

bool GameScene::humanToMove() const {
    if (!_started || _game.status().over())
        return false;
    return !againstComputer() || _game.position().sideToMove() == _human;
}

void GameScene::play(const Move& move, bool slide) {
    if (!_game.play(move))
        return;
    eventlog::note("move " + move.uci());

    _input.reset();
    _hintFrom = _hintTo = kNoSquare;
    _hintText.clear();
    _hintWanted = false;  // a hint still being worked out was for a position that is gone

    _board->show(_game.position(), _game.lastMove(), slide);
    refresh();
    analyse();
    askEngineIfItsTurn();
}

void GameScene::askEngineIfItsTurn() {
    _replyPending = false;
    if (!_started || !againstComputer() || _game.status().over() ||
        _game.position().sideToMove() == _human) {
        return;
    }

    if (!_engine.usable()) {
        _engineLost = true;
        refreshStatus();
        return;
    }

    const std::string key = _game.uciPosition();

    // Has it been here before in this game — the player took a move back and
    // then made the same one again? Then it answers as it did. See _replies.
    if (const auto known = _replies.find(key); known != _replies.end()) {
        _replyPending = true;
        _replyMove    = known->second;
        _replyDelay   = kReplyPause;
        _engine.cancel();
        _moveTag = 0;
        return;
    }

    const Level level = levelAtStrength(_setup.strength);

    // The handicap of the lowest levels: this move is not the engine's.
    std::uniform_real_distribution<double> chance(0.0, 1.0);
    if (chance(_rng) < level.randomMoveShare) {
        _replyPending = true;
        _replyMove.clear();
        _replyDelay = kReplyPause + 0.35f * static_cast<float>(chance(_rng));
        _engine.cancel();
        _moveTag = 0;
        return;
    }

    const auto elo = _engine.spin("UCI_Elo");

    SearchRequest request;
    request.position   = key;
    request.moveTimeMs = level.moveTimeMs;
    request.options    = engineOptions(level, elo ? elo->min : 0, elo ? elo->max : 0, hardwareThreads());
    request.tag = _moveTag = _nextTag++;
    _moveKey    = key;
    _engine.search(std::move(request));
}

namespace {
// Random, not perverse: a pawn that reaches the end becomes a queen.
std::optional<Move> randomMove(const Position& position, std::mt19937& rng) {
    std::vector<Move> moves = position.legalMoves();
    moves.erase(std::remove_if(moves.begin(), moves.end(),
                               [](const Move& move) {
                                   return move.promotion != PieceType::None &&
                                          move.promotion != PieceType::Queen;
                               }),
                moves.end());
    if (moves.empty())
        return std::nullopt;
    std::uniform_int_distribution<std::size_t> pick(0, moves.size() - 1);
    return moves[pick(rng)];
}
}  // namespace

void GameScene::playRandomMove() {
    if (const auto move = randomMove(_game.position(), _rng))
        play(*move, true);
}

void GameScene::playPendingReply() {
    _replyPending = false;

    const Position& position = _game.position();
    std::optional<Move> move = _replyMove.empty() ? std::nullopt : position.parseUci(_replyMove);
    if (!move)
        move = randomMove(position, _rng);
    if (!move)
        return;

    // Remembered before it is played: afterwards the position is a different one.
    _replies[_game.uciPosition()] = move->uci();
    play(*move, true);
}

void GameScene::pollEngine() {
    SearchResult result;
    while (_engine.poll(result)) {
        if (_moveTag == 0 || result.tag != _moveTag)
            continue;  // an answer to a question that has since been withdrawn
        _moveTag = 0;

        if (const auto move = _game.position().parseUci(result.bestMove)) {
            _replies[_moveKey] = result.bestMove;
            play(*move, true);
        } else {
            // The engine was given this very game move by move, so a move
            // that is not legal here means the two of us disagree about the
            // rules. Better to stop than to guess.
            AXLOGW("engine move '{}' is not legal in {}", result.bestMove, _game.position().fen());
            _engineLost = true;
            refresh();
        }
    }

    // "Starting" became "ready" (or something worse): what the panel says
    // about the engine may no longer be true.
    const Engine::State state = _engine.state();
    if (state != _engineState) {
        _engineState = state;
        refresh();
    }

    // An engine that dies in the middle of a game says nothing — its output
    // just ends. Notice, and tell the player, instead of "thinking" for ever.
    if (againstComputer() && _started && !_engineLost && _engine.state() == Engine::State::Failed) {
        AXLOGW("engine lost: {}", _engine.error());
        _engineLost = true;
        _moveTag    = 0;
        refresh();
    }
}

// --- the engine that watches -------------------------------------------------------

// A search is about to be abandoned: whatever it has found so far is still
// worth having. True if that changed the marks.
bool GameScene::keepWhatWasFound() {
    SearchResult result;
    return _analysisTag != 0 && _analysis.progress(result) && result.tag == _analysisTag &&
           _review.note(_searchKey, result, false);
}

void GameScene::marksChanged() {
    showMoves();
    refreshMarks();
}

void GameScene::analyse() {
    const bool marked = keepWhatWasFound();
    _analysis.cancel();
    _analysisTag  = 0;
    _analysisDone = false;
    _analysisBest.clear();
    _searchKey.clear();

    if (_review.follow(_game) || marked)
        marksChanged();

    if (!_started) {
        _evalBar->show({});
        return;
    }

    // A finished game needs no opinion: the result is the evaluation. The
    // moves that led to it may still be waiting for theirs.
    const Status status = _game.status();
    if (status.over()) {
        EvalBar::Score score;
        score.kind = !status.winner                   ? EvalBar::Score::Kind::Drawn
                     : *status.winner == Color::White ? EvalBar::Score::Kind::WhiteWon
                                                      : EvalBar::Score::Kind::BlackWon;
        _evalBar->show(score);
        reviewNext();
        return;
    }

    if (!_analysis.usable())
        return;

    // Been here before — a move was taken back? Then it is already known.
    if (const auto* known = _review.found(_review.now()); known != nullptr && known->complete) {
        _analysisDone = true;
        _analysisBest = known->best;
        showScore(known->score);
        if (_hintWanted)
            showHint();
        reviewNext();
        return;
    }

    // The bar keeps showing the previous position's score until this one has
    // something to say. One move rarely changes the picture much, and a bar
    // that jumped back to the middle after every move would be all flicker.
    SearchRequest request;
    request.position   = _review.now();
    request.moveTimeMs = kAnalysisMs;
    request.depth      = kAnalysisDepth;
    request.options    = analysisOptions();
    request.tag = _analysisTag = _nextTag++;
    _searchKey = _review.now();
    _analysis.search(std::move(request));
}

// With nothing more urgent to look at, the engine goes back over the game:
// the most recent position it did not get a proper look at, then the one
// before. A move is marked as soon as the positions either side of it are in.
void GameScene::reviewNext() {
    if (!_analysis.usable() || _analysisTag != 0 || !_started)
        return;
    auto request = _review.nextToReview();
    if (!request)
        return;
    request->options = analysisOptions();
    request->tag = _analysisTag = _nextTag++;
    _searchKey = request->position;
    _analysis.search(std::move(*request));
}

void GameScene::showScore(const Score& found) {
    // The engine scores for whoever is to move; the bar is always white's view.
    const int sign = _game.position().sideToMove() == Color::White ? 1 : -1;

    EvalBar::Score score;
    if (found.mate && *found.mate != 0) {
        score.kind  = EvalBar::Score::Kind::Mate;
        score.value = sign * *found.mate;
    } else if (found.cp) {
        score.kind  = EvalBar::Score::Kind::Centipawns;
        score.value = sign * *found.cp;
    } else {
        return;
    }
    _evalBar->show(score);
}

void GameScene::pollAnalysis() {
    SearchResult result;
    // Is the search in progress about the position on the board, or one of
    // the earlier ones being gone back over?
    const auto onBoard = [this] {
        return _searchKey == _review.now() && !_game.status().over();
    };

    // While it is still looking at the position on the board: follow along.
    if (onBoard() && _analysisTag != 0 && _analysis.progress(result) && result.tag == _analysisTag &&
        result.depth >= kAnalysisMinDepth) {
        showScore(Score{result.scoreCp, result.mateIn});
    }

    while (_analysis.poll(result)) {
        if (_analysisTag == 0 || result.tag != _analysisTag)
            continue;
        _analysisTag      = 0;
        const bool marked = _review.note(_searchKey, result, true);

        if (onBoard()) {
            _analysisDone = true;
            _analysisBest = result.bestMove;
            showScore(Score{result.scoreCp, result.mateIn});
            if (_hintWanted)
                showHint();
        }
        _searchKey.clear();
        if (marked)
            marksChanged();
        refresh();
        reviewNext();
    }

    // The bar is only on screen while there is an engine behind it. If that
    // changes — it has just come up, or it has died — the board gets the
    // space back or gives it up.
    if (_analysis.usable() != _barShown) {
        layout();
        if (_barShown && _analysisTag == 0 && !_analysisDone)
            analyse();
    }
}

void GameScene::showHint() {
    _hintWanted = false;
    if (const auto move = _game.position().parseUci(_analysisBest)) {
        _hintFrom = move->from;
        _hintTo   = move->to;
        _hintText = text::localSan(_game.position().san(*move));
    }
}

void GameScene::update(float delta) {
    pollEngine();
    pollAnalysis();

    if (_replyPending) {
        _replyDelay -= delta;
        if (_replyDelay <= 0.0f)
            playPendingReply();
    }

    Launch& options = launch();
    if (options.autoplayGames > 0 && _started && modal() == nullptr) {
        if (_game.status().over()) {
            AXLOGI("autoplay: {} after {} plies", text::verdict(_game.status(), std::nullopt).headline,
                   _game.plies());
            if (--options.autoplayGames == 0)
                Director::getInstance()->end();
            else
                startGame(_setup);
        } else if (humanToMove() && !_board->animating()) {
            playRandomMove();
        }
    }

    const bool barMoving = _evalBar->step(delta);

    if (_copiedFor > 0.0f) {
        _copiedFor -= delta;
        if (_copiedFor <= 0.0f) {
            _panel->copy->setText(text::kCopyAll);
            _panel->markMove(-1);
        }
    }

    // Is anything going to change without the player touching anything? Each
    // of these is a reason to keep drawing, and the list has to be complete:
    // once the game is asleep only the window can wake it (see Wakefulness.h).
    _awake.update(delta, {
        {_engine.thinking(), "the opponent is thinking"},
        {_analysis.thinking(), "the position is being analysed"},
        {_replyPending, "a reply is about to be played"},
        {barMoving, "the evaluation bar is moving"},
        {_board->animating(), "a piece is moving"},
        {_input.pressed(), "the mouse button is down"},
        {_panel->dragging(), "the move list is being dragged"},
        {_copiedFor > 0.0f, "the \"copied\" notice is up"},
        {_engine.state() == Engine::State::Starting, "the opponent is starting"},
        {_analysis.state() == Engine::State::Starting, "the analysis engine is starting"},
        {!options.screenshot.empty(), "a screenshot is being taken"},
        {options.autoplayGames > 0, "autoplay"},
    });
}

// --- what the player can do --------------------------------------------------------

bool GameScene::gameAtStake() const {
    return _started && _game.plies() > 0 && !_game.status().over() && launch().autoplayGames == 0;
}

void GameScene::openNewGame() {
    if (!gameAtStake()) {
        showNewGameDialog();
        return;
    }
    _input.cancelPromotion();
    _quitAsked = false;
    _confirm->open(text::kUnfinishedTitle, text::kLoseByNewGame, text::kNewGame, [this] { showNewGameDialog(); });
    keepAwake();
}

// The window has been asked to close. With a game in progress the answer is a
// question; asked a second time while that question is still up, it is yes.
void GameScene::askBeforeQuitting(bool& closing) {
    if (!gameAtStake() || (_quitAsked && _confirm->isOpen()))
        return;
    closing    = false;
    _quitAsked = true;
    _confirm->open(
        text::kUnfinishedTitle, text::kLoseByQuit, text::kQuit, [] { Director::getInstance()->end(); },
        [this] { _quitAsked = false; });
    keepAwake();
}

void GameScene::showNewGameDialog() {
    _input.cancelPromotion();
    // The dialog is also the place to find out that Stockfish is back.
    if (!_engine.usable() || !_analysis.usable())
        startEngine();
    const auto elo = _engine.spin("UCI_Elo");
    // A game from a position set up in the editor: the side to move there is
    // the one offered to the player.
    GameSetup offered = _setup;
    if (_startFrom)
        offered.side = _startFrom->sideToMove() == Color::White ? GameSetup::Side::White : GameSetup::Side::Black;
    _dialog->open(offered, _started, _engine.usable(), elo ? elo->min : 0, elo ? elo->max : 0);
    keepAwake();
}

void GameScene::openLooks() {
    _input.cancelPromotion();
    _looks->open();
    keepAwake();
}

// The player has picked other pieces or another board. The position, the
// marks on it and the captured pieces beside it are all drawn again as they
// were, in the new look.
void GameScene::restyle() {
    _board->restyle();
    _top->restyle();
    _bottom->restyle();
    keepAwake();
}

// --- the clipboard -----------------------------------------------------------------

namespace {

// What goes into a PGN is data for other programs, not text for the player,
// so it is not in Strings.h and not in Ukrainian.
constexpr const char* kPgnEvent  = "Casual game";
constexpr const char* kPgnPlayer = "Player";

std::string today() {
    const std::time_t now = std::time(nullptr);
    char text[16]         = "";
    if (const std::tm* local = std::localtime(&now))
        std::strftime(text, sizeof(text), "%Y.%m.%d", local);
    return text;
}

}  // namespace

void GameScene::copyGame() {
    if (_game.plies() == 0)
        return;

    PgnTags tags;
    tags.event = kPgnEvent;
    tags.date  = today();
    if (againstComputer()) {
        // A level by its number; a strength between two by what was asked
        // of the engine there.
        const Level level  = levelAtStrength(_setup.strength);
        std::string engine = "Stockfish, ";
        if (level.exact) {
            engine += "level " + std::to_string(level.number) + " of " + std::to_string(levels().size());
        } else {
            engine += std::to_string(opponentElo(level)) + " Elo";
            if (level.randomMoveShare > 0.0)
                engine += ", " + std::to_string(std::lround(level.randomMoveShare * 100.0)) + "% random moves";
        }
        tags.white = _human == Color::White ? kPgnPlayer : engine;
        tags.black = _human == Color::Black ? kPgnPlayer : engine;
    }
    std::vector<std::string> marks;
    for (const Mark mark : _review.marks())
        marks.emplace_back(suffix(mark));
    desktop::copy(_game.pgn(tags, marks));
    _panel->markMove(-1);
    showCopied();
}

void GameScene::copyMove(int index) {
    if (index < 0 || index >= static_cast<int>(_game.plies()))
        return;
    // As it is written in the list, with its number and its mark: "12. Кf3",
    // "12... Кf6?!".
    const Game::Record& record = _game.history()[static_cast<std::size_t>(index)];
    const std::string number   = std::to_string(record.before.fullmoveNumber()) +
                                 (record.before.sideToMove() == Color::White ? ". " : "... ");
    const Mark mark = _review.mark(static_cast<std::size_t>(index));
    desktop::copy(number + text::localSan(record.san) + suffix(mark));
    _panel->markMove(index);
    showCopied();
}

void GameScene::showCopied() {
    _panel->copy->setText(text::kCopied);
    _copiedFor = kCopiedSeconds;
    keepAwake();
}

void GameScene::undo() {
    if (!_started)
        return;
    _input.cancelPromotion();
    _input.cancelPress();

    _engine.cancel();
    _moveTag      = 0;
    _replyPending = false;
    _hintWanted   = false;

    if (_game.status().outcome == Outcome::Resignation) {
        _game.undo();  // only takes the resignation back
    } else if (!againstComputer()) {
        _game.undo();
    } else {
        // Back to the player's own previous turn: their move, and the reply to
        // it if there was one.
        do {
            if (!_game.undo())
                break;
        } while (_game.position().sideToMove() != _human);
    }

    _input.reset();
    _hintFrom = _hintTo = kNoSquare;
    _hintText.clear();
    _board->show(_game.position());
    eventlog::note("undo " + _game.position().fen());
    refresh();
    analyse();
    askEngineIfItsTurn();
}

void GameScene::flip() {
    _input.cancelPromotion();
    _input.cancelPress();
    _board->setFlipped(!_board->flipped());
    _evalBar->setFlipped(_board->flipped());
    refresh();
}

void GameScene::askHint() {
    if (!humanToMove() || !_analysis.usable() || _hintWanted)
        return;

    // The hint is the watching engine's own choice. It has usually finished
    // with this position long before anyone asks; if not, the hint appears the
    // moment it does.
    _hintWanted = true;
    if (_analysisDone)
        showHint();
    else if (_analysisTag == 0)
        analyse();
    refresh();
}

void GameScene::resign() {
    if (!_started || _game.status().over())
        return;
    _input.cancelPromotion();
    _input.cancelPress();
    _engine.cancel();
    _moveTag      = 0;
    _replyPending = false;
    _hintWanted   = false;

    // Against the computer it is always the person who resigns, whoever is to
    // move; at a shared board it is whoever's turn it is.
    _game.resign(againstComputer() ? _human : _game.position().sideToMove());
    eventlog::note("resign");
    _input.reset();
    _hintFrom = _hintTo = kNoSquare;
    _hintText.clear();
    refresh();
    analyse();
}

// --- input -------------------------------------------------------------------------

Dialog* GameScene::modal() const {
    if (_confirm->isOpen())
        return _confirm;
    if (_dialog->isOpen())
        return _dialog;
    if (_looks->isOpen())
        return _looks;
    return nullptr;
}

void GameScene::onMouseDown(const Vec2& point) {
    keepAwake();

    if (Dialog* dialog = modal()) {
        dialog->mouseDown(point);
        return;
    }

    if (_input.promotionClick(point))
        return;

    if (_buttons.mouseDown(point))
        return;

    // The scroll bar of the move list, or one of the moves in it.
    if (_panel->mouseDown(point))
        return;

    // Off the board, or not the player's turn: the selection goes, unless the
    // click was on the panel (copying a move, say).
    if (!_input.mouseDown(point) && !_panel->contains(point))
        _input.select(kNoSquare);
}

void GameScene::onMouseMove(const Vec2& point) {
    // The pointer crossing the window is not, by itself, a reason to draw:
    // only if it lights a button up or lets one go, or is carrying a piece.
    if (Dialog* dialog = modal()) {
        if (dialog->mouseMove(point))
            keepAwake();
        return;
    }
    _buttons.mouseMove(point);
    const bool carrying = _input.mouseMove(point);
    if (_buttons.hoverChanged() || carrying)
        keepAwake();
    // The move list: a move lit up or let go, the thumb dragged along.
    if (_panel->mouseMove(point))
        keepAwake();
}

void GameScene::onMouseUp(const Vec2& point) {
    keepAwake();

    if (Dialog* dialog = modal()) {
        dialog->mouseUp(point);
        // A dialog that opened while the button was down (a key) took the
        // release: whatever was held under it is let go of, not acted on.
        _input.cancelPress();
        if (_panel->dragging())
            _panel->mouseUp(point);
        return;
    }
    _buttons.mouseUp(point);
    _panel->mouseUp(point);
    _input.mouseUp(point);
}

void GameScene::onKeyPressed(EventKeyboard::KeyCode code) {
    using Key = EventKeyboard::KeyCode;
    keepAwake();

    if (Dialog* dialog = modal()) {
        dialog->keyPressed(code);
        return;
    }

    switch (code) {
    case Key::KEY_ESCAPE:
        if (_board->promotionOpen())
            _input.cancelPromotion();
        else
            _input.select(kNoSquare);
        break;
    case Key::KEY_N:
        openNewGame();
        break;
    case Key::KEY_U:
    case Key::KEY_BACKSPACE:
    case Key::KEY_LEFT_ARROW:
        if (_panel->undo->enabled())
            undo();
        break;
    case Key::KEY_F:
        flip();
        break;
    case Key::KEY_H:
        if (_panel->hint->enabled())
            askHint();
        break;
    case Key::KEY_V:
        openLooks();
        break;
    case Key::KEY_P:
        openPuzzles();
        break;
    case Key::KEY_C:
        copyGame();
        break;
    default:
        break;
    }
}

// --- putting the state on screen ---------------------------------------------------

void GameScene::onWindowResized() {
    keepAwake();

    auto* view = Director::getInstance()->getRenderView();
    if (view == nullptr)
        return;

    // Design resolution = frame size, so the scene is never scaled to fit: a
    // larger window shows a larger board rather than the same picture
    // stretched, and the text stays as crisp as it was.
    const Vec2 frame = view->getFrameSize();
    if (frame.x < 1.0f || frame.y < 1.0f)
        return;
    view->setDesignResolutionSize(frame.x, frame.y, ResolutionPolicy::SHOW_ALL);

    layout();
}

void GameScene::layout() {
    const Vec2 screen = Director::getInstance()->getVisibleSize();

    _backdrop->clear();
    shapes::rect(_backdrop, Vec2::ZERO, screen, theme::kBackground);

    // Nothing here is a fixed pixel position. The panel takes a share of the
    // width between two limits, and the board takes the largest square that
    // fits in what is left with a name strip above and below it.
    const float panelWidth = std::clamp(std::floor(screen.x * 0.27f), 300.0f, 380.0f);
    const float areaWidth  = screen.x - panelWidth;

    // The evaluation bar stands to the left of the board, and only while
    // there is an engine to fill it; without one the board has the room.
    _barShown             = _analysis.usable();
    const float evalSpace = _barShown ? kEvalWidth + kEvalGap : 0.0f;

    float side = std::min(areaWidth - 2.0f * kMargin - evalSpace, screen.y - 2.0f * (kMargin + kBarHeight));
    // A whole number of pixels per square, so every edge between two squares
    // lands on a pixel boundary.
    side = std::max(160.0f, std::floor(side / 8.0f) * 8.0f);
    const float frame = look::frameWidth(side / 8.0f);

    // Bar and board are centred together, as one thing.
    const Vec2 corner(std::floor((areaWidth - side - evalSpace) * 0.5f) + evalSpace,
                      std::floor((screen.y - side) * 0.5f));
    _board->setPosition(corner);
    _board->setSide(side);

    _evalBar->setVisible(_barShown);
    _evalBar->setFrame(Vec2(corner.x - frame - kEvalGap - kEvalWidth, corner.y), Vec2(kEvalWidth, side));

    _bottom->setFrame(Vec2(corner.x, corner.y - frame - kBarHeight), Vec2(side, kBarHeight));
    _top->setFrame(Vec2(corner.x, corner.y + side + frame), Vec2(side, kBarHeight));

    _panel->setFrame(Vec2(areaWidth, 0.0f), Vec2(panelWidth, screen.y));
    _dialog->layout(screen);
    _looks->layout(screen);
    _confirm->layout(screen);
}

void GameScene::refresh() {
    refreshMarks();
    refreshBars();
    refreshStatus();

    showMoves();

    const Status status = _game.status();

    // Taking a move back needs a move of the player's own to take back — or a
    // resignation to think better of.
    bool canUndo = status.outcome == Outcome::Resignation;
    for (const Game::Record& record : _game.history())
        canUndo |= !againstComputer() || record.before.sideToMove() == _human;

    _panel->undo->setEnabled(_started && canUndo);
    _panel->hint->setEnabled(humanToMove() && _analysis.usable() && !_hintWanted);
    _panel->resign->setEnabled(_started && !status.over());
}

// The move list: every move as it is written here, and the mark it has earned.
void GameScene::showMoves() {
    std::vector<std::string> moves;
    moves.reserve(_game.plies());
    for (const Game::Record& record : _game.history())
        moves.push_back(text::localSan(record.san));
    _panel->setMoves(moves, _game.startPosition().sideToMove() == Color::White,
                     _game.startPosition().fullmoveNumber());

    std::vector<Mark> marks = _review.marks();
    marks.resize(moves.size(), Mark::None);  // the list may be a move ahead of the marks, or behind
    _panel->setMarks(marks);
    _panel->setResult(text::resultSign(_game.status()));
}

void GameScene::refreshMarks() {
    BoardNode::Marks marks;

    if (const Move* last = _game.lastMove()) {
        marks.lastFrom = last->from;
        marks.lastTo   = last->to;
    }

    const Position& position = _game.position();
    if (position.inCheck())
        marks.check = position.kingSquare(position.sideToMove());

    // What the state of the game says about the kings.
    using Sign = BoardNode::Sign;
    const auto sign = [&](Color king, Sign what) {
        marks.kings[king == Color::White ? 0 : 1] = {position.kingSquare(king), what};
    };
    const Status status = _game.status();
    switch (status.outcome) {
    case Outcome::Ongoing:
        if (_started && position.inCheck())
            sign(position.sideToMove(), Sign::Check);
        break;
    case Outcome::Checkmate:
    case Outcome::Resignation:
        if (status.winner) {
            sign(*status.winner, Sign::Won);
            sign(opposite(*status.winner),
                 status.outcome == Outcome::Checkmate ? Sign::Mate : Sign::Resigned);
        }
        break;
    case Outcome::Stalemate:
    case Outcome::FiftyMoves:
    case Outcome::Repetition:
    case Outcome::InsufficientMaterial:
        sign(Color::White, Sign::Draw);
        sign(Color::Black, Sign::Draw);
        break;
    }

    if (const Square selected = _input.selected(); selected != kNoSquare && humanToMove()) {
        marks.selected = selected;
        marks.targets  = position.legalMovesFrom(selected);
    }

    marks.hintFrom = _hintFrom;
    marks.hintTo   = _hintTo;

    // What the last move was worth, on the square it went to.
    if (const auto& verdicts = _review.marks(); !verdicts.empty() && verdicts.size() == _game.plies())
        marks.verdict = verdicts.back();
    _board->setMarks(marks);
}

void GameScene::refreshBars() {
    // Whoever is at the bottom of the board gets the bottom bar.
    const Color below = _board->flipped() ? Color::Black : Color::White;
    const Color above = opposite(below);

    const int balance  = _game.materialBalance();  // white minus black
    const bool playing = _started && !_game.status().over();

    const auto fill = [&](PlayerBar* bar, Color colour) {
        std::string name;
        if (!_started || !againstComputer())
            name = colour == Color::White ? text::kWhite : text::kBlack;
        else if (colour == _human)
            name = text::kYou;
        else
            name = std::string(text::kComputer) + " · " + text::levelName(levelAtStrength(_setup.strength).number);

        const int ahead = colour == Color::White ? balance : -balance;
        bar->set(name, colour, _game.capturedPieces(opposite(colour)), text::lead(ahead),
                 playing && _game.position().sideToMove() == colour);
    };
    fill(_bottom, below);
    fill(_top, above);
}

void GameScene::refreshStatus() {
    using Tone = SidePanel::Tone;

    if (!_started) {
        _panel->setSubtitle("");
        _panel->setStatus(text::kNewGame, "", Tone::Neutral);
        return;
    }

    if (againstComputer()) {
        const Level level = levelAtStrength(_setup.strength);
        _panel->setSubtitle(text::strengthLabel(level, opponentElo(level)));
    } else {
        _panel->setSubtitle(text::kTwoPlayers);
    }

    const Status status = _game.status();
    if (status.over()) {
        const text::Verdict verdict =
            text::verdict(status, againstComputer() ? std::optional<Color>(_human) : std::nullopt);
        _panel->setStatus(verdict.headline, verdict.detail, verdict.happy ? Tone::Good : Tone::Neutral);
        return;
    }

    const Position& position = _game.position();
    const std::string check  = position.inCheck() ? text::kCheck : "";

    if (!againstComputer()) {
        _panel->setStatus(position.sideToMove() == Color::White ? text::kWhiteToMove : text::kBlackToMove,
                          check);
        return;
    }

    if (_engineLost) {
        _panel->setStatus(text::kEngineLost, text::kEngineLostHelp, Tone::Warning);
        return;
    }

    if (position.sideToMove() == _human) {
        std::string detail = check;
        if (_hintWanted)
            detail = text::kHintThinking;
        else if (!_hintText.empty())
            detail = std::string(text::kHint) + ": " + _hintText;
        _panel->setStatus(text::kYourMove, detail);
    } else {
        _panel->setStatus(_engine.state() == Engine::State::Starting ? text::kEngineStarting : text::kThinking,
                          check);
    }
}

void GameScene::captureAndQuit() {
    // The picture is taken once the screen has stopped changing: the engine
    // has answered, the piece has landed, the bar has settled and every move
    // that is going to get a mark has it.
    desktop::screenshotWhenSettled(this, launch().screenshot, [this] {
        return _engine.thinking() || _analysis.thinking() || _replyPending || _board->animating() ||
               _engine.state() == Engine::State::Starting || _analysis.state() == Engine::State::Starting;
    });
}
