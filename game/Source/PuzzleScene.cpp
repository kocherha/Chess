#include "PuzzleScene.h"

#include "Look.h"
#include "Shapes.h"
#include "Strings.h"
#include "Text.h"
#include "Theme.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

using namespace ax;
using namespace chess;

#ifndef CHESS_ENGINE_FALLBACK
#    define CHESS_ENGINE_FALLBACK ""
#endif

namespace {

// As in the game: after half a second with nothing to wait for, stop drawing.
constexpr float kSleepAfter    = 0.5f;
constexpr float kDragThreshold = 5.0f;

constexpr float kMargin    = 26.0f;
constexpr float kTrayShare = 0.82f;  // a tray's height, in squares
constexpr float kTrayGap   = 10.0f;

constexpr float kProbeDelay   = 0.3f;   // after the last edit, before the engine is asked
constexpr float kReplyPause   = 0.45f;  // before the defence is played
constexpr float kWrongSeconds = 1.8f;   // a wrong move stays on the board this long
constexpr float kFlashSeconds = 1.6f;

// What the engine is asked, and for how long.
constexpr int kProbeMate = 8;      // the longest mate the editor looks for
constexpr int kProbeMs   = 3000;
constexpr int kShorterMs = 2000;   // then: is there a shorter one?
constexpr int kCheckMs   = 1500;   // does this move keep the mate?
constexpr int kRecheckMs = 1000;   // the same question, asked of the defence
constexpr int kReplyMs   = 700;
constexpr int kHintMs    = 3000;
constexpr int kBestMs    = 2500;   // best-move problems: the best move, and the player's
constexpr int kBestDepth = 18;
// A move is as good as the best if it gives away less than this much of the
// chance to win (see chess::winChance).
constexpr double kBestTolerance = 0.03;

// One thread. With more, Stockfish sometimes ends a search restricted to a
// single move ("searchmoves") after a few plies, without the mate it would
// have found — and the verdict on the player's move is exactly that search.
std::vector<std::pair<std::string, std::string>> puzzleOptions() {
    return {{"Threads", "1"}, {"MultiPV", "1"}, {"UCI_LimitStrength", "false"}, {"Skill Level", "20"}};
}

void toClipboard(const std::string& text) {
#if defined(AX_PLATFORM_PC)
    glfwSetClipboardString(nullptr, text.c_str());
#else
    (void)text;
#endif
}

std::string fromClipboard() {
#if defined(AX_PLATFORM_PC)
    if (const char* text = glfwGetClipboardString(nullptr))
        return text;
#endif
    return {};
}

}  // namespace

PuzzleScene* PuzzleScene::create(Hooks hooks, Opening opening) {
    auto* scene = new PuzzleScene();
    if (scene->initWith(std::move(hooks), std::move(opening))) {
        scene->autorelease();
        return scene;
    }
    delete scene;
    return nullptr;
}

PuzzleScene::~PuzzleScene() {
    _engine.shutdown();
}

// --- setting up --------------------------------------------------------------------

bool PuzzleScene::initWith(Hooks hooks, Opening opening) {
    if (!Scene::init())
        return false;
    _hooks   = std::move(hooks);
    _opening = std::move(opening);

    _backdrop = DrawNode::create();
    _board    = BoardNode::create();
    _top      = PieceTray::create(Color::Black);
    _bottom   = PieceTray::create(Color::White);
    _caption  = typeset::label("", 15.0f, theme::kDim);
    _panel    = PuzzlePanel::create();
    _list     = PuzzleDialog::create();
    _looks    = LookDialog::create();
    _confirm  = ConfirmDialog::create();
    addChild(_backdrop, -1);
    addChild(_board, 1);
    addChild(_top, 1);
    addChild(_bottom, 1);
    addChild(_caption, 1);
    addChild(_panel, 2);
    addChild(_list, 10);
    addChild(_looks, 10);
    addChild(_confirm, 11);

    // The panel.
    _panel->looks->onClick = [this] {
        _looks->open();
        keepAwake();
    };
    _panel->modeGame->onClick   = [this] {
        if (_hooks.backToGame)
            _hooks.backToGame();
    };
    _panel->collection->onClick = [this] { openCollection(); };

    _panel->onSideToMove = [this](Color colour) {
        _setup.setSideToMove(colour);
        edited();
    };
    _panel->onCastling = [this](int index) {
        const Color colour  = index < 2 ? Color::White : Color::Black;
        const bool kingSide = index % 2 == 0;
        _setup.setCastling(colour, kingSide, !_setup.castlingWanted(colour, kingSide));
        edited();
    };
    _panel->onEnPassant = [this](Square square) {
        _setup.setEnPassant(square);
        edited();
    };
    _panel->start->onClick = [this] {
        _setup = Setup::initial();
        edited();
    };
    _panel->clear->onClick = [this] {
        _setup.clear();
        edited();
    };
    _panel->flip->onClick        = [this] { flip(); };
    _panel->flipSolving->onClick = [this] { flip(); };
    _panel->copyFen->onClick     = [this] { copyFen(); };
    _panel->pasteFen->onClick    = [this] { pasteFen(); };
    _panel->saveMine->onClick    = [this] { saveMine(); };
    _panel->solve->onClick       = [this] { solveSetup(); };
    _panel->playFrom->onClick    = [this] { playFromSetup(); };

    _panel->next->onClick     = [this] { openPuzzle(puzzles::next(_puzzle.id)); };
    _panel->hint->onClick     = [this] { askHint(); };
    _panel->solution->onClick = [this] { showSolution(); };
    _panel->undo->onClick     = [this] { takeBack(); };
    _panel->restart->onClick  = [this] { restart(); };
    _panel->edit->onClick     = [this] { openEditor(Setup::from(_game.startPosition())); };
    _panel->removeMine->onClick = [this] {
        const std::string fen = _puzzle.fen;
        puzzles::remove(_puzzle.id);
        // What was the problem stays on the board, in the editor, in case it
        // was deleted by mistake: it can be saved again from there.
        openEditor(Setup::fromFen(fen).value_or(Setup::initial()));
    };

    _looks->onChange = [this] { restyle(); };
    _looks->onDone   = [this] { _looks->close(); };
    _list->onChoose  = [this](const Puzzle& puzzle) {
        _list->close();
        openPuzzle(puzzle);
    };
    _list->onCancel = [this] { _list->close(); };

    // Input: one listener each, as in the game.
    auto* mouse        = EventListenerMouse::create();
    mouse->onMouseDown = [this](EventMouse* event) {
        const auto button = event->getMouseButton();
        if (button == EventMouse::MouseButton::BUTTON_LEFT || button == EventMouse::MouseButton::BUTTON_RIGHT)
            onMouseDown(event->getLocation(), button == EventMouse::MouseButton::BUTTON_RIGHT);
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
    _eventDispatcher->addEventListenerWithSceneGraphPriority(mouse, this);

    auto* keyboard         = EventListenerKeyboard::create();
    keyboard->onKeyPressed = [this](EventKeyboard::KeyCode code, Event*) { onKeyPressed(code); };
    _eventDispatcher->addEventListenerWithSceneGraphPriority(keyboard, this);

#if defined(AX_PLATFORM_PC)
    const auto whenWindow = [this](std::string_view event, std::function<void(EventCustom*)> what) {
        _eventDispatcher->addEventListenerWithSceneGraphPriority(
            EventListenerCustom::create(event, std::move(what)), this);
    };
    whenWindow(RenderViewImpl::EVENT_WINDOW_POSITIONED, [this](EventCustom*) { keepAwake(); });
    whenWindow(RenderViewImpl::EVENT_WINDOW_FOCUSED, [this](EventCustom*) { keepAwake(); });
    whenWindow(RenderViewImpl::EVENT_WINDOW_RESIZED, [this](EventCustom*) {
        keepAwake();
        if (auto* view = Director::getInstance()->getRenderView()) {
            const Vec2 frame = view->getFrameSize();
            if (frame.x >= 1.0f && frame.y >= 1.0f)
                view->setDesignResolutionSize(frame.x, frame.y, ResolutionPolicy::SHOW_ALL);
        }
        layout();
    });
    whenWindow(RenderViewImpl::EVENT_WINDOW_CLOSE, [this](EventCustom* event) {
        if (auto* closing = static_cast<bool*>(event->getUserData()))
            askBeforeQuitting(*closing);
    });
#endif

    // The engine: the same Stockfish the game plays with, a process of this
    // scene's own.
    if (const std::string path = findEngine(CHESS_ENGINE_FALLBACK); !path.empty()) {
        if (std::getenv("CHESS_UCI_LOG") != nullptr)
            _engine.setTrace([](char direction, const std::string& line) {
                std::fprintf(stderr, "puzzle %c %s\n", direction, line.c_str());
            });
        if (!_engine.start(path))
            AXLOGW("puzzle engine did not start: {}", _engine.error());
    }

    layout();

    const Puzzle chosen = _opening.puzzle.empty() ? Puzzle{} : puzzles::find(_opening.puzzle);
    if (!chosen.fen.empty()) {
        openPuzzle(chosen);
    } else {
        if (!_opening.puzzle.empty())
            AXLOGW("--puzzle ignored: there is none called {}", _opening.puzzle);
        std::optional<Setup> setup;
        if (!_opening.fen.empty() && !(setup = Setup::fromFen(_opening.fen)))
            AXLOGW("--edit ignored: not a FEN");
        openEditor(setup.value_or(Setup::initial()));
    }

    scheduleUpdate();
    if (!_opening.screenshot.empty())
        captureAndQuit();
    return true;
}

void PuzzleScene::onEnter() {
    Scene::onEnter();
    layout();
    keepAwake();
}

bool PuzzleScene::engineUsable() const {
    const Engine::State state = _engine.state();
    return state == Engine::State::Starting || state == Engine::State::Ready;
}

// --- the engine --------------------------------------------------------------------

void PuzzleScene::ask(Question question, SearchRequest request) {
    request.options = puzzleOptions();
    request.tag = _tag = _nextTag++;
    _question       = question;
    _engine.search(std::move(request));
}

void PuzzleScene::pollEngine() {
    SearchResult result;
    while (_engine.poll(result)) {
        if (_tag == 0 || result.tag != _tag)
            continue;
        const Question question = _question;
        _question = Question::None;
        _tag      = 0;

        switch (question) {
        case Question::Probe:
        case Question::Shorter: {
            if (_page != Page::Editor || _setup.fen() != _probeFen)
                break;
            if (question == Question::Probe) {
                _probeDone = true;
                _probeMate = result.mateIn && *result.mateIn != 0 ? result.mateIn : std::nullopt;
                _probeCp   = result.scoreCp;
            } else if (result.mateIn && *result.mateIn > 0 && _probeMate && *result.mateIn < *_probeMate) {
                _probeMate = result.mateIn;
            } else {
                refreshEditor();
                break;  // nothing shorter
            }
            // A mate was found; the search stops at the first one it sees,
            // which need not be the quickest. Ask for one a move shorter
            // until there is none.
            if (_probeMate && *_probeMate > 1) {
                SearchRequest shorter;
                shorter.position   = "position fen " + _probeFen;
                shorter.mate       = *_probeMate - 1;
                shorter.moveTimeMs = kShorterMs;
                ask(Question::Shorter, shorter);
            }
            refreshEditor();
            break;
        }

        case Question::Prepare:
            if (_page != Page::Solving)
                break;
            _bestMove  = result.bestMove;
            _bestScore = Score{result.scoreCp, result.mateIn};
            _phase     = Phase::Ready;
            // "Show the solution" was asked for while the engine was still
            // looking: now it has the move, play it.
            if (_autoSolve) {
                if (const auto move = _game.position().parseUci(_bestMove)) {
                    played(*move, true);
                    break;
                }
                _autoSolve = false;  // nothing to show: the player's move again
            }
            refreshSolving();
            break;

        case Question::Check: {
            if (_page != Page::Solving || _phase != Phase::Checking)
                break;
            const std::string move = _game.history().empty() ? "" : text::localSan(_game.history().back().san);
            if (_puzzle.mateIn <= 0) {
                // Best-move problems: as good as the best, or near enough.
                const Score mine{result.scoreCp, result.mateIn};
                const bool  same  = _checkMove == _bestMove;
                const bool  right = same || !_bestScore.known() ||
                                   (mine.known() && winChance(mine) >= winChance(_bestScore) - kBestTolerance);
                const auto  best  = _game.history().empty() ? std::nullopt
                                                            : _game.history().back().before.parseUci(_bestMove);
                const std::string bestSan =
                    best ? text::localSan(_game.history().back().before.san(*best)) : std::string();
                if (right) {
                    _marks.back()  = Mark::Great;
                    _verdictDetail = text::bestMoveVerdict(move, true, bestSan);
                    finish(true);
                } else {
                    wrong(text::bestMoveVerdict(move, false, bestSan));
                }
                break;
            }
            const int before = movesLeft() + 1;
            if (result.mateIn && *result.mateIn > 0 && *result.mateIn <= before) {
                SearchRequest defence;
                defence.position   = _game.uciPosition();
                defence.moveTimeMs = kReplyMs;
                defence.depth      = 2 * movesLeft() + 10;
                ask(Question::Reply, defence);
                _phase         = Phase::Replying;
                _verdictDetail = text::rightMove(move);
                if (attackerMoves() == 1)
                    _marks.back() = Mark::Great;
            } else {
                // Not found from the attacking side. Before calling it wrong,
                // ask the defence: is it mated within the moves that are left?
                SearchRequest recheck;
                recheck.position   = _game.uciPosition();
                recheck.moveTimeMs = kRecheckMs;
                recheck.depth      = 2 * movesLeft() + 12;
                ask(Question::Recheck, recheck);
            }
            refreshSolving();
            break;
        }

        case Question::Recheck: {
            if (_page != Page::Solving || _phase != Phase::Checking)
                break;
            const std::string move = text::localSan(_game.history().back().san);
            if (result.mateIn && *result.mateIn < 0 && -*result.mateIn <= movesLeft()) {
                _verdictDetail = text::rightMove(move);
                if (attackerMoves() == 1)
                    _marks.back() = Mark::Great;
                reply(result.bestMove);
            } else {
                wrong(text::wrongMove(move, movesLeft() + 1));
            }
            break;
        }

        case Question::Reply:
            if (_page == Page::Solving && _phase == Phase::Replying)
                reply(result.bestMove);
            break;

        case Question::Hint: {
            if (_page != Page::Solving || !(_phase == Phase::Ready && _game.position().sideToMove() == _attacker))
                break;
            const auto move = _game.position().parseUci(result.bestMove);
            if (!move)
                break;
            if (_autoSolve) {
                played(*move, true);
                break;
            }
            _hintFrom      = move->from;
            _hintTo        = move->to;
            _verdictDetail = std::string(text::kHint) + ": " + text::localSan(_game.position().san(*move));
            refreshSolving();
            break;
        }

        case Question::None:
            break;
        }
    }
}

// --- the editor --------------------------------------------------------------------

void PuzzleScene::openEditor(const Setup& setup) {
    _engine.cancel();
    _question = Question::None;
    _tag      = 0;
    _page     = Page::Editor;
    _phase    = Phase::Ready;
    _autoSolve = false;
    _setup    = setup;
    _brush    = {};
    _selected = _hintFrom = _hintTo = kNoSquare;
    cancelPress();
    _board->closePromotion();
    _top->setChosen({});
    _bottom->setChosen({});
    _top->setVisible(true);
    _bottom->setVisible(true);
    _caption->setVisible(false);
    _panel->setPage(PuzzlePanel::Page::Editor);
    layout();
    edited();
}

std::optional<Position> PuzzleScene::setupPosition() const {
    return _setup.position();
}

void PuzzleScene::edited() {
    // Whatever the engine was saying is about a position that is gone.
    if (_question == Question::Probe || _question == Question::Shorter) {
        _engine.cancel();
        _question = Question::None;
        _tag      = 0;
    }
    _probeDone = false;
    _probeMate.reset();
    _probeCp.reset();
    _probeFen.clear();

    const auto position = setupPosition();
    _probeIn = position && position->hasLegalMoves() && engineUsable() ? kProbeDelay : -1.0f;
    refreshEditor();
    keepAwake();
}

void PuzzleScene::probe() {
    _probeIn = -1.0f;
    const auto position = setupPosition();
    if (_page != Page::Editor || !position || !position->hasLegalMoves() || !engineUsable())
        return;
    _probeFen = _setup.fen();
    SearchRequest request;
    request.position   = "position fen " + _probeFen;
    request.mate       = kProbeMate;
    request.moveTimeMs = kProbeMs;
    ask(Question::Probe, request);
    refreshEditor();
}

// A piece goes down on a square. There is one king of a colour: putting one
// where there is already one elsewhere moves him.
void PuzzleScene::place(Square square, Piece piece) {
    if (piece.type == PieceType::King) {
        for (Square other = 0; other < 64; ++other)
            if (other != square && _setup.at(other) == piece)
                _setup.remove(other);
    }
    _setup.put(square, piece);
}

void PuzzleScene::refreshEditor() {
    if (_page != Page::Editor)
        return;

    _board->show(BoardNode::Placement([this] {
        BoardNode::Placement placement;
        for (Square square = 0; square < 64; ++square)
            placement[static_cast<std::size_t>(square)] = _setup.at(square);
        return placement;
    }()));

    _panel->setSideToMove(_setup.sideToMove());
    std::array<bool, 4> wanted{}, possible{};
    for (int i = 0; i < 4; ++i) {
        const Color colour  = i < 2 ? Color::White : Color::Black;
        const bool kingSide = i % 2 == 0;
        wanted[static_cast<std::size_t>(i)]   = _setup.castlingWanted(colour, kingSide);
        possible[static_cast<std::size_t>(i)] = _setup.castlingPossible(colour, kingSide);
    }
    _panel->setCastling(wanted, possible);
    _panel->setEnPassant(_setup.enPassantCandidates(), _setup.enPassant());
    _panel->setFen(_setup.fen());

    using Tone = PuzzlePanel::Tone;
    const auto issues   = _setup.issues();
    const auto position = issues.empty() ? setupPosition() : std::nullopt;
    const bool playable = position && position->hasLegalMoves();

    if (!issues.empty()) {
        std::string detail;
        bool onlyKings = true;
        for (const Issue& issue : issues) {
            onlyKings = onlyKings && issue.flaw == Flaw::MissingKing;
            detail += (detail.empty() ? "" : "\n") + text::issue(issue);
        }
        _panel->setStatus(onlyKings ? text::kUnfinished : text::kImpossible, detail,
                          onlyKings ? Tone::Warning : Tone::Bad);
    } else if (!position) {
        _panel->setStatus(text::kImpossible, "", Tone::Bad);
    } else if (!playable) {
        _panel->setStatus(text::kPossible, text::alreadyOver(position->sideToMove(), position->inCheck()),
                          Tone::Neutral);
    } else if (!engineUsable()) {
        _panel->setStatus(text::kPossible, text::kNoEngineHere, Tone::Good);
    } else if (!_probeDone) {
        _panel->setStatus(text::kPossible, text::kStudying, Tone::Good);
    } else {
        std::string detail = text::finding(position->sideToMove(), _probeMate, _probeCp);
        if (!_probeMate || *_probeMate < 0)
            detail += std::string("\n") + text::kNoForcedMate;
        _panel->setStatus(text::kPossible, detail, Tone::Good);
    }

    const bool studied = playable && (_probeDone || !engineUsable());
    _panel->solve->setEnabled(studied && engineUsable());
    _panel->saveMine->setEnabled(studied);
    _panel->playFrom->setEnabled(playable);

    refreshMarks();
}

void PuzzleScene::copyFen() {
    toClipboard(_setup.fen());
    flash(_panel->copyFen, text::kCopied, [this] { _panel->copyFen->setText(text::kCopyFen); });
}

void PuzzleScene::pasteFen() {
    // Whatever is on the clipboard, with the spaces at either end taken off.
    std::string clip = fromClipboard();
    const auto first = clip.find_first_not_of(" \t\r\n");
    clip = first == std::string::npos ? "" : clip.substr(first, clip.find_last_not_of(" \t\r\n") - first + 1);
    if (const auto setup = Setup::fromFen(clip)) {
        _setup = *setup;
        edited();
        flash(_panel->pasteFen, text::kPasted, [this] { _panel->pasteFen->setText(text::kPasteFen); });
    } else {
        flash(_panel->pasteFen, text::kNoFen, [this] { _panel->pasteFen->setText(text::kPasteFen); });
    }
}

void PuzzleScene::saveMine() {
    if (!_panel->saveMine->enabled())
        return;
    puzzles::save(_setup.fen(), _probeMate && *_probeMate > 0 ? *_probeMate : 0);
    flash(_panel->saveMine, text::kSaved, [this] { _panel->saveMine->setText(text::kSaveMine); });
}

void PuzzleScene::solveSetup() {
    if (!_panel->solve->enabled())
        return;
    // A position already among the player's own is that problem — so that
    // solving it counts.
    const std::string fen = _setup.fen();
    for (const Puzzle& puzzle : puzzles::mine())
        if (puzzle.fen == fen) {
            Puzzle again = puzzle;
            again.mateIn = _probeMate && *_probeMate > 0 ? *_probeMate : 0;
            openPuzzle(again);
            return;
        }
    Puzzle puzzle;
    puzzle.title  = text::kFromEditor;
    puzzle.fen    = fen;
    puzzle.mateIn = _probeMate && *_probeMate > 0 ? *_probeMate : 0;
    openPuzzle(puzzle);
}

void PuzzleScene::playFromSetup() {
    if (const auto position = setupPosition(); position && position->hasLegalMoves() && _hooks.playFrom)
        _hooks.playFrom(*position);
}

// --- solving -----------------------------------------------------------------------

void PuzzleScene::openPuzzle(const Puzzle& puzzle) {
    std::string error;
    const auto position = Position::fromFen(puzzle.fen, &error);
    if (!position) {
        AXLOGW("puzzle {} is not a legal position: {}", puzzle.id, error);
        openEditor(Setup::fromFen(puzzle.fen).value_or(Setup::initial()));
        return;
    }

    _engine.cancel();
    _question = Question::None;
    _tag      = 0;

    _page      = Page::Solving;
    _puzzle    = puzzle;
    _game      = Game(*position);
    _attacker  = position->sideToMove();
    _marks.clear();
    _helped    = false;
    _autoSolve = false;
    _bestMove.clear();
    _bestScore = {};
    _verdictDetail.clear();
    _replyMove.clear();
    _wrongFor   = 0.0f;
    _replyDelay = 0.0f;
    _selected = _hintFrom = _hintTo = kNoSquare;
    _promoteFrom = _promoteTo = kNoSquare;
    cancelPress();
    _board->closePromotion();
    _brush = {};

    _top->setVisible(false);
    _bottom->setVisible(false);
    _caption->setVisible(true);
    _panel->setPage(PuzzlePanel::Page::Solving);
    _panel->setMine(puzzle.mine);
    _panel->setPuzzle(puzzle.title, puzzle.note, text::goal(_attacker, puzzle.mateIn));

    // The side that solves sits at the bottom.
    _board->setFlipped(_attacker == Color::Black);
    layout();
    _board->show(_game.position());

    _phase = Phase::Ready;
    if (puzzle.mateIn <= 0 && engineUsable()) {
        _phase = Phase::Preparing;
        SearchRequest request;
        request.position   = _game.uciPosition();
        request.depth      = kBestDepth;
        request.moveTimeMs = kBestMs;
        ask(Question::Prepare, request);
    }
    refreshSolving();
    keepAwake();
}

void PuzzleScene::restart() {
    const Puzzle puzzle = _puzzle;
    openPuzzle(puzzle);
}

int PuzzleScene::attackerMoves() const {
    int count = 0;
    for (const Game::Record& record : _game.history())
        count += record.before.sideToMove() == _attacker ? 1 : 0;
    return count;
}

int PuzzleScene::movesLeft() const {
    return _puzzle.mateIn - attackerMoves();
}

bool PuzzleScene::playerToMove() const {
    return _page == Page::Solving && _phase == Phase::Ready && !_autoSolve &&
           _game.position().sideToMove() == _attacker && !_game.status().over();
}

// A move by the side solving — the player's, or the engine's when it is
// showing the solution — is on the board. Now: is it right?
void PuzzleScene::played(const Move& move, bool slide) {
    const std::string before = _game.uciPosition();
    if (!_game.play(move))
        return;
    _checkKey  = before;
    _checkMove = move.uci();
    _marks.resize(_game.plies() - 1, Mark::None);
    _marks.push_back(Mark::None);
    _selected = _hintFrom = _hintTo = kNoSquare;
    _board->show(_game.position(), _game.lastMove(), slide);

    const Status status    = _game.status();
    const std::string said = text::localSan(_game.history().back().san);

    if (_puzzle.mateIn > 0) {
        if (status.outcome == Outcome::Checkmate) {
            finish(!_autoSolve);
            return;
        }
        if (status.over()) {
            wrong(text::verdict(status, std::nullopt).headline + ".");
            return;
        }
        if (movesLeft() <= 0) {
            wrong(text::wrongMove(said, 1));
            return;
        }
        if (!engineUsable()) {
            wrong(text::kNoEngineHere);
            return;
        }
        if (_autoSolve) {
            // The engine's own move: no need to ask whether it is right.
            _verdictDetail = text::rightMove(said);
            SearchRequest defence;
            defence.position   = _game.uciPosition();
            defence.moveTimeMs = kReplyMs;
            defence.depth      = 2 * movesLeft() + 10;
            ask(Question::Reply, defence);
            _phase = Phase::Replying;
            refreshSolving();
            return;
        }
        SearchRequest check;
        check.position    = before;
        check.mate        = movesLeft() + 1;
        check.moveTimeMs  = kCheckMs;
        check.searchMoves = {_checkMove};
        ask(Question::Check, check);
        _phase = Phase::Checking;
        refreshSolving();
        return;
    }

    // The best move: one move and done.
    if (_autoSolve || !engineUsable()) {
        _marks.back()  = Mark::Great;
        _verdictDetail = text::bestMoveVerdict(said, true, "");
        finish(false);
        return;
    }
    SearchRequest check;
    check.position    = before;
    check.depth       = kBestDepth;
    check.moveTimeMs  = kBestMs;
    check.searchMoves = {_checkMove};
    ask(Question::Check, check);
    _phase = Phase::Checking;
    refreshSolving();
}

void PuzzleScene::reply(const std::string& uci) {
    const auto move = _game.position().parseUci(uci);
    if (!move) {
        _phase = Phase::Ready;
        refreshSolving();
        return;
    }
    _replyMove  = uci;
    _replyDelay = kReplyPause;
    _phase      = Phase::Replying;
    refreshSolving();
    keepAwake();
}

void PuzzleScene::wrong(const std::string& why) {
    _engine.cancel();
    _question = Question::None;
    _tag      = 0;
    if (!_marks.empty())
        _marks.back() = Mark::Mistake;
    _phase         = Phase::Wrong;
    _wrongFor      = kWrongSeconds;
    _verdictDetail = why;
    refreshSolving();
    keepAwake();
}

void PuzzleScene::finish(bool byPlayer) {
    _phase = Phase::Solved;
    if (byPlayer && !_helped && !_puzzle.id.empty())
        puzzles::markSolved(_puzzle.id);
    if (_puzzle.mateIn > 0)
        _verdictDetail = !byPlayer ? text::kSolutionDone : _helped ? text::kSolvedHelped : text::kSolvedAlone;
    _autoSolve = false;
    refreshSolving();
}

// Back to the solver's previous turn: the reply and the move before it, or
// just the move if it was the last one played.
void PuzzleScene::takeBack() {
    if (_page != Page::Solving || _game.plies() == 0 || _phase == Phase::Preparing)
        return;
    _engine.cancel();
    _question  = Question::None;
    _tag       = 0;
    _autoSolve = false;
    _replyMove.clear();
    _wrongFor = 0.0f;
    cancelPress();

    do {
        _game.undo();
    } while (_game.plies() > 0 && _game.position().sideToMove() != _attacker);
    _marks.resize(_game.plies());
    _selected = _hintFrom = _hintTo = kNoSquare;
    _verdictDetail.clear();
    _phase = Phase::Ready;
    _board->show(_game.position());
    refreshSolving();
    keepAwake();
}

void PuzzleScene::askHint() {
    if (!playerToMove() || !engineUsable())
        return;
    _helped = true;
    if (_puzzle.mateIn <= 0) {
        if (const auto move = _game.position().parseUci(_bestMove)) {
            _hintFrom      = move->from;
            _hintTo        = move->to;
            _verdictDetail = std::string(text::kHint) + ": " + text::localSan(_game.position().san(*move));
        }
        refreshSolving();
        return;
    }
    SearchRequest request;
    request.position   = _game.uciPosition();
    request.mate       = movesLeft();
    request.moveTimeMs = kHintMs;
    ask(Question::Hint, request);
    _verdictDetail = text::kHintLooking;
    refreshSolving();
}

void PuzzleScene::showSolution() {
    if (_page != Page::Solving || !engineUsable() || _phase == Phase::Preparing)
        return;
    if (_phase == Phase::Solved || _phase == Phase::Wrong) {
        // From the start, so the whole solution is seen.
        const Puzzle puzzle = _puzzle;
        openPuzzle(puzzle);
        if (_page != Page::Solving)
            return;  // it could not be set up again and went to the editor
    }
    // After openPuzzle(), which clears both.
    _autoSolve = true;
    _helped    = true;
    _hintFrom = _hintTo = kNoSquare;
    if (_phase == Phase::Preparing) {
        // A best-move problem started again: the engine is looking for the
        // best move, and plays it as soon as it has it (Question::Prepare).
        refreshSolving();
        return;
    }
    if (_phase == Phase::Ready && _game.position().sideToMove() == _attacker) {
        if (_puzzle.mateIn <= 0) {
            if (const auto move = _game.position().parseUci(_bestMove)) {
                played(*move, true);
                return;
            }
            // The engine never named a best move here: nothing to show.
            _autoSolve = false;
            refreshSolving();
            return;
        }
        SearchRequest request;
        request.position   = _game.uciPosition();
        request.mate       = movesLeft();
        request.moveTimeMs = kHintMs;
        ask(Question::Hint, request);
    }
    refreshSolving();
}

std::string PuzzleScene::movesText() const {
    std::string out;
    const auto& history = _game.history();
    for (std::size_t i = 0; i < history.size(); ++i) {
        const Position& before = history[i].before;
        if (before.sideToMove() == Color::White)
            out += (out.empty() ? "" : " ") + std::to_string(before.fullmoveNumber()) + ". ";
        else if (i == 0)
            out += std::to_string(before.fullmoveNumber()) + "... ";
        else
            out += ' ';
        out += text::localSan(history[i].san);
        if (i < _marks.size())
            out += suffix(_marks[i]);
    }
    return out;
}

void PuzzleScene::refreshSolving() {
    if (_page != Page::Solving)
        return;
    using Tone = PuzzlePanel::Tone;

    _panel->setMoves(movesText());
    switch (_phase) {
    case Phase::Preparing:
        _panel->setStatus(text::kFindingBest, "", Tone::Neutral);
        break;
    case Phase::Ready:
        if (_autoSolve)
            _panel->setStatus(text::kShowingSolution, _verdictDetail, Tone::Neutral);
        else if (!engineUsable())
            _panel->setStatus(text::kYourMove, text::kNoEngineHere, Tone::Warning);
        else
            _panel->setStatus(text::kYourMove, _verdictDetail, Tone::Neutral);
        break;
    case Phase::Checking:
        _panel->setStatus(text::kChecking, "", Tone::Neutral);
        break;
    case Phase::Replying:
        _panel->setStatus(_autoSolve ? text::kShowingSolution : text::kRight, _verdictDetail,
                          _autoSolve ? Tone::Neutral : Tone::Good);
        break;
    case Phase::Wrong:
        _panel->setStatus(text::kWrong, _verdictDetail + " " + text::kTryAgain, Tone::Bad);
        break;
    case Phase::Solved:
        _panel->setStatus(_helped ? text::kSolution : text::kSolved, _verdictDetail, _helped ? Tone::Neutral : Tone::Good);
        break;
    }

    const bool busy = _phase == Phase::Checking || _phase == Phase::Replying || _phase == Phase::Preparing;
    _panel->hint->setEnabled(playerToMove() && engineUsable());
    _panel->solution->setEnabled(engineUsable() && !busy && !_autoSolve);
    _panel->undo->setEnabled(_game.plies() > 0 && !busy && !_autoSolve);
    _panel->restart->setEnabled(_game.plies() > 0 || _helped);

    const Status status = _game.status();
    _caption->setString(status.over() ? "" : text::toMoveCaption(_game.position().sideToMove()));
    refreshMarks();
}

// --- input -------------------------------------------------------------------------

Dialog* PuzzleScene::modal() const {
    if (_confirm->isOpen())
        return _confirm;
    if (_list->isOpen())
        return _list;
    if (_looks->isOpen())
        return _looks;
    return nullptr;
}

void PuzzleScene::select(Square square) {
    if (_selected == square)
        return;
    _selected = square;
    refreshMarks();
}

bool PuzzleScene::tryMove(Square from, Square to, bool carried) {
    const Position& position = _game.position();
    if (position.isPromotion(from, to)) {
        _promoteFrom    = from;
        _promoteTo      = to;
        _promoteCarried = carried;
        _board->openPromotion(to, position.sideToMove());
        return true;
    }
    const auto move = position.findMove(from, to);
    if (!move)
        return false;
    played(*move, !carried);
    return true;
}

void PuzzleScene::onMouseDown(const Vec2& point, bool right) {
    keepAwake();

    if (Dialog* dialog = modal()) {
        if (!right)
            dialog->mouseDown(point);
        return;
    }

    if (_board->promotionOpen()) {
        const PieceType piece = _board->promotionAt(point);
        const Square from = _promoteFrom, to = _promoteTo;
        const bool carried = _promoteCarried;
        _promoteFrom = _promoteTo = kNoSquare;
        _board->closePromotion();
        if (piece != PieceType::None)
            if (const auto move = _game.position().findMove(from, to, piece))
                played(*move, !carried);
        select(kNoSquare);
        return;
    }

    if (right) {
        // The quick way to take a piece off in the editor.
        if (_page == Page::Editor) {
            const Square square = _board->squareAt(point);
            if (square != kNoSquare && !_setup.at(square).empty()) {
                _setup.remove(square);
                edited();
            }
        }
        return;
    }

    if (_panel->buttons().mouseDown(point))
        return;

    _pressPoint = point;
    _dragging   = false;

    if (_page == Page::Editor) {
        const PieceTray::Item item = !_top->at(point).none() ? _top->at(point) : _bottom->at(point);
        if (!item.none()) {
            _pressed     = true;
            _pressItem   = item;
            _pressSquare = kNoSquare;
            return;
        }
        const Square square = _board->squareAt(point);
        if (square != kNoSquare) {
            _pressed     = true;
            _pressItem   = {};
            _pressSquare = square;
        }
        return;
    }

    // Solving: as in the game.
    const Square square = _board->squareAt(point);
    if (square == kNoSquare || !playerToMove()) {
        select(kNoSquare);
        return;
    }
    if (_selected != kNoSquare && _selected != square && tryMove(_selected, square, false))
        return;
    const Piece piece = _game.position().at(square);
    if (!piece.empty() && piece.color == _game.position().sideToMove()) {
        _wasSelected = _selected == square;
        select(square);
        _pressed     = true;
        _pressItem   = {};
        _pressSquare = square;
    } else {
        select(kNoSquare);
    }
}

void PuzzleScene::onMouseMove(const Vec2& point) {
    if (Dialog* dialog = modal()) {
        if (dialog->mouseMove(point))
            keepAwake();
        return;
    }
    _panel->buttons().mouseMove(point);
    if (_panel->buttons().hoverChanged() || _pressed)
        keepAwake();

    if (!_pressed)
        return;
    if (!_dragging && point.distance(_pressPoint) > kDragThreshold) {
        if (!_pressItem.none()) {
            if (!_pressItem.eraser) {
                // A piece lifted off the tray: a sprite of its own, over everything.
                _carried = look::sprite(_pressItem.piece, _board->squareSize() * 1.12f);
                if (_carried != nullptr) {
                    addChild(_carried, 5);
                    _dragging = true;
                }
            }
        } else if (_pressSquare != kNoSquare) {
            const bool hasPiece = _page == Page::Editor ? !_setup.at(_pressSquare).empty()
                                                        : !_game.position().at(_pressSquare).empty();
            if (hasPiece) {
                _dragging = true;
                _board->beginDrag(_pressSquare, point);
            }
        }
    }
    if (_dragging) {
        if (_carried != nullptr)
            _carried->setPosition(point);
        else
            _board->dragTo(point);
    }
}

void PuzzleScene::onMouseUp(const Vec2& point) {
    keepAwake();
    if (Dialog* dialog = modal()) {
        dialog->mouseUp(point);
        // A dialog that opened while the button was down (a key) took the
        // release: whatever was held under it is let go of, not acted on.
        cancelPress();
        return;
    }
    _panel->buttons().mouseUp(point);

    if (!_pressed)
        return;
    _pressed = false;
    const bool dragged = _dragging;
    _dragging          = false;

    if (_page == Page::Editor) {
        if (!_pressItem.none()) {
            if (dragged) {
                dropFromTray(point);
            } else {
                // A click on the tray picks the brush, or puts it down again.
                _brush = _brush == _pressItem ? PieceTray::Item{} : _pressItem;
                _top->setChosen(_brush);
                _bottom->setChosen(_brush);
            }
            _pressItem = {};
            return;
        }

        const Square from = _pressSquare;
        _pressSquare      = kNoSquare;
        if (dragged) {
            // The board was shown afresh under the hand (a right click took a
            // piece off, say): the piece was put back, there is nothing to drop.
            if (!_board->dragging())
                return;
            const Square target = _board->squareAt(point);
            _board->endDrag();
            const Piece piece = _setup.at(from);
            if (target == from)
                return;
            // Off the board: gone. Onto another square: moved there.
            _setup.remove(from);
            if (target != kNoSquare)
                place(target, piece);
            edited();
            return;
        }
        if (from == kNoSquare || _brush.none())
            return;
        // A click with a brush: put it there — or, if it is there already, take it off.
        if (_brush.eraser || _setup.at(from) == _brush.piece)
            _setup.remove(from);
        else
            place(from, _brush.piece);
        edited();
        return;
    }

    if (dragged) {
        if (!_board->dragging())
            return;  // shown afresh under the hand: nothing to drop
        const Square target = _board->squareAt(point);
        _board->endDrag();
        if (target != kNoSquare && target != _pressSquare && playerToMove())
            tryMove(_pressSquare, target, true);
    } else if (_wasSelected) {
        select(kNoSquare);
    }
}

void PuzzleScene::cancelPress() {
    if (_carried != nullptr) {
        _carried->removeFromParent();
        _carried = nullptr;
    }
    _board->endDrag();
    _pressed = _dragging = false;
    _pressItem   = {};
    _pressSquare = kNoSquare;
}

void PuzzleScene::dropFromTray(const Vec2& point) {
    if (_carried != nullptr) {
        _carried->removeFromParent();
        _carried = nullptr;
    }
    const Square target = _board->squareAt(point);
    if (target == kNoSquare || _pressItem.eraser)
        return;
    place(target, _pressItem.piece);
    edited();
}

void PuzzleScene::onKeyPressed(EventKeyboard::KeyCode code) {
    using Key = EventKeyboard::KeyCode;
    keepAwake();
    if (Dialog* dialog = modal()) {
        dialog->keyPressed(code);
        return;
    }

    switch (code) {
    case Key::KEY_ESCAPE:
        if (_board->promotionOpen()) {
            _board->closePromotion();
            _promoteFrom = _promoteTo = kNoSquare;
        }
        _brush = {};
        _top->setChosen(_brush);
        _bottom->setChosen(_brush);
        select(kNoSquare);
        break;
    case Key::KEY_F:
        flip();
        break;
    case Key::KEY_V:
        _looks->open();
        break;
    case Key::KEY_L:
        openCollection();
        break;
    case Key::KEY_G:
        if (_hooks.backToGame)
            _hooks.backToGame();
        break;
    case Key::KEY_U:
    case Key::KEY_BACKSPACE:
    case Key::KEY_LEFT_ARROW:
        if (_page == Page::Solving && _panel->undo->enabled())
            takeBack();
        break;
    case Key::KEY_H:
        if (_page == Page::Solving)
            askHint();
        break;
    case Key::KEY_N:
        if (_page == Page::Solving)
            openPuzzle(puzzles::next(_puzzle.id));
        break;
    case Key::KEY_R:
        if (_page == Page::Solving)
            restart();
        break;
    default:
        break;
    }
}

// --- the screen --------------------------------------------------------------------

void PuzzleScene::openCollection() {
    _list->open();
    keepAwake();
}

void PuzzleScene::flip() {
    cancelPress();
    _board->setFlipped(!_board->flipped());
    layout();
    refreshMarks();
    keepAwake();
}

void PuzzleScene::restyle() {
    _board->restyle();
    _top->restyle();
    _bottom->restyle();
    keepAwake();
}

void PuzzleScene::flash(Button* button, const char* text, std::function<void()> restore) {
    if (_unflash)
        _unflash();
    button->setText(text);
    _unflash  = std::move(restore);
    _flashFor = kFlashSeconds;
    keepAwake();
}

void PuzzleScene::askBeforeQuitting(bool& closing) {
    if (!_hooks.gameAtStake || !_hooks.gameAtStake() || (_quitAsked && _confirm->isOpen()))
        return;
    closing    = false;
    _quitAsked = true;
    _confirm->open(
        text::kUnfinishedTitle, text::kLoseByQuit, text::kQuit, [] { Director::getInstance()->end(); },
        [this] { _quitAsked = false; });
    keepAwake();
}

void PuzzleScene::layout() {
    const Vec2 screen = Director::getInstance()->getVisibleSize();

    _backdrop->clear();
    shapes::rect(_backdrop, Vec2::ZERO, screen, theme::kBackground);

    const float panelWidth = std::clamp(std::floor(screen.x * 0.27f), 300.0f, 380.0f);
    const float areaWidth  = screen.x - panelWidth;

    // The board with a tray above and below it, as one column.
    const float share = kTrayShare / 8.0f;
    float side = std::min(areaWidth - 2.0f * kMargin, (screen.y - 2.0f * kMargin - 2.0f * kTrayGap) / (1.0f + 2.0f * share));
    side = std::max(160.0f, std::floor(side / 8.0f) * 8.0f);
    const float frame = look::frameWidth(side / 8.0f);
    const float tray  = std::floor(side / 8.0f * kTrayShare);

    const Vec2 corner(std::floor((areaWidth - side) * 0.5f), std::floor((screen.y - side) * 0.5f));
    _board->setPosition(corner);
    _board->setSide(side);

    // White's tray on white's side of the board.
    const Vec2 below(corner.x, corner.y - frame - kTrayGap - tray);
    const Vec2 above(corner.x, corner.y + side + frame + kTrayGap);
    const bool flipped = _board->flipped();
    _bottom->setFrame(flipped ? above : below, Vec2(side, tray));
    _top->setFrame(flipped ? below : above, Vec2(side, tray));
    _caption->setPosition(Vec2(corner.x + side * 0.5f, corner.y - frame - kTrayGap - tray * 0.5f));

    _panel->setFrame(Vec2(areaWidth, 0.0f), Vec2(panelWidth, screen.y));
    _list->layout(screen);
    _looks->layout(screen);
    _confirm->layout(screen);
}

void PuzzleScene::refreshMarks() {
    BoardNode::Marks marks;

    if (_page == Page::Editor) {
        for (const Issue& issue : _setup.issues())
            for (Square square : issue.squares)
                marks.problems.push_back(square);
        _board->setMarks(marks);
        return;
    }

    if (const Move* last = _game.lastMove()) {
        marks.lastFrom = last->from;
        marks.lastTo   = last->to;
    }
    const Position& position = _game.position();
    if (position.inCheck())
        marks.check = position.kingSquare(position.sideToMove());

    using Sign = BoardNode::Sign;
    const Status status = _game.status();
    if (status.outcome == Outcome::Checkmate && status.winner) {
        marks.kings[0] = {position.kingSquare(*status.winner), Sign::Won};
        marks.kings[1] = {position.kingSquare(opposite(*status.winner)), Sign::Mate};
    } else if (status.over()) {
        marks.kings[0] = {position.kingSquare(Color::White), Sign::Draw};
        marks.kings[1] = {position.kingSquare(Color::Black), Sign::Draw};
    } else if (position.inCheck()) {
        marks.kings[0] = {position.kingSquare(position.sideToMove()), Sign::Check};
    }

    if (_selected != kNoSquare && playerToMove()) {
        marks.selected = _selected;
        marks.targets  = position.legalMovesFrom(_selected);
    }
    marks.hintFrom = _hintFrom;
    marks.hintTo   = _hintTo;
    if (!_marks.empty() && _marks.size() == _game.plies())
        marks.verdict = _marks.back();
    _board->setMarks(marks);
}

// --- the frame ---------------------------------------------------------------------

void PuzzleScene::update(float delta) {
    pollEngine();

    if (_probeIn >= 0.0f) {
        _probeIn -= delta;
        if (_probeIn < 0.0f)
            probe();
    }

    if (!_replyMove.empty()) {
        _replyDelay -= delta;
        if (_replyDelay <= 0.0f) {
            const std::string uci = std::move(_replyMove);
            _replyMove.clear();
            if (const auto move = _game.position().parseUci(uci); move && _phase == Phase::Replying) {
                _game.play(*move);
                _marks.resize(_game.plies(), Mark::None);
                _board->show(_game.position(), _game.lastMove(), true);
                _phase = Phase::Ready;
                if (_autoSolve) {
                    SearchRequest request;
                    request.position   = _game.uciPosition();
                    request.mate       = std::max(1, movesLeft());
                    request.moveTimeMs = kHintMs;
                    ask(Question::Hint, request);
                }
                refreshSolving();
            }
        }
    }

    if (_phase == Phase::Wrong && _wrongFor > 0.0f) {
        _wrongFor -= delta;
        if (_wrongFor <= 0.0f) {
            _wrongFor = 0.0f;
            cancelPress();
            _game.undo();
            _marks.resize(_game.plies());
            _board->show(_game.position());
            _phase = Phase::Ready;
            refreshSolving();
        }
    }

    if (_flashFor > 0.0f) {
        _flashFor -= delta;
        if (_flashFor <= 0.0f && _unflash) {
            const auto restore = std::move(_unflash);
            _unflash = nullptr;
            restore();
        }
    }

    // The engine's state can change by itself: started, or died.
    if (_engine.state() != _engineState) {
        _engineState = _engine.state();
        if (_page == Page::Editor)
            edited();
        else
            refreshSolving();
    }

    const bool waiting = _engine.thinking() || _probeIn >= 0.0f || !_replyMove.empty() ||
                         (_phase == Phase::Wrong && _wrongFor > 0.0f) || _board->animating() || _pressed ||
                         _flashFor > 0.0f || _engine.state() == Engine::State::Starting ||
                         !_opening.screenshot.empty();
    _quiet = waiting ? 0.0f : _quiet + delta;
    if (_quiet > kSleepAfter) {
        _quiet = 0.0f;
        Director::getInstance()->stopAnimation();
    }
}

void PuzzleScene::keepAwake() {
    _quiet = 0.0f;
    auto* director = Director::getInstance();
    if (!director->isValid())
        director->startAnimation();
}

void PuzzleScene::captureAndQuit() {
    schedule(
        [this](float delta) {
            static float waited = 0.0f, settled = 0.0f;
            waited += delta;
            const bool busy = _engine.thinking() || _probeIn >= 0.0f || !_replyMove.empty() || _board->animating() ||
                              _engine.state() == Engine::State::Starting;
            settled = busy ? 0.0f : settled + delta;
            if (waited < 1.5f || (settled < 0.6f && waited < 120.0f))
                return;
            unschedule("screenshot");
            utils::captureScreen(
                [](bool ok, std::string_view path) {
                    AXLOGI("screenshot {}: {}", ok ? "written" : "FAILED", path);
                    Director::getInstance()->end();
                },
                _opening.screenshot);
        },
        0.25f, "screenshot");
}
