#include "PuzzleScene.h"

#include "Desktop.h"
#include "EventLog.h"
#include "Look.h"
#include "Shapes.h"
#include "Stockfish.h"
#include "Strings.h"
#include "Text.h"
#include "Theme.h"

#include <algorithm>
#include <cmath>

using namespace ax;
using namespace chess;


namespace {

constexpr float kMargin    = 26.0f;
constexpr float kTrayShare = 0.82f;  // a tray's height, in squares
constexpr float kTrayGap   = 10.0f;

constexpr float kProbeDelay   = 0.3f;   // after the last edit, before the engine is asked
constexpr float kFlashSeconds = 1.6f;

// What the editor asks the engine, and for how long. (What solving asks is
// chess::PuzzleSession's.)
constexpr int kProbeMate = 8;      // the longest mate the editor looks for
constexpr int kProbeMs   = 3000;
constexpr int kShorterMs = 2000;   // then: is there a shorter one?

// One thread. With more, Stockfish sometimes ends a search restricted to a
// single move ("searchmoves") after a few plies, without the mate it would
// have found — and the verdict on the player's move is exactly that search.
std::vector<std::pair<std::string, std::string>> puzzleOptions() {
    return {{"Threads", "1"}, {"MultiPV", "1"}, {"UCI_LimitStrength", "false"}, {"Skill Level", "20"}};
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
    _input.attach(_board);
    _input.position = [this]() -> const Position& { return _session.game().position(); };
    _input.mayMove  = [this] { return playerToMove(); };
    _input.play     = [this](const Move& move, bool slide) {
        if (_session.play(move, slide))
            sessionChanged();
    };
    _input.selectionChanged = [this] { refreshMarks(); };
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
    _panel->edit->onClick     = [this] { openEditor(Setup::from(_session.game().startPosition())); };
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

    desktop::followWindow(this, {.wake    = [this] { keepAwake(); },
                                 .resized = [this] { layout(); },
                                 .closing = [this](bool& closing) { askBeforeQuitting(closing); }});

    // The engine: the same Stockfish the game plays with, a process of this
    // scene's own.
    if (const std::string path = stockfish::path(); !path.empty()) {
        stockfish::traceIfAsked(_engine, "puzzle");
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

// --- the engine --------------------------------------------------------------------

void PuzzleScene::ask(Question question, SearchRequest request) {
    request.options = puzzleOptions();
    request.tag = _tag = _nextTag--;  // negative: the session's are positive
    _question       = question;
    _engine.search(std::move(request));
}

void PuzzleScene::talkToEngine() {
    auto work = _session.toEngine();
    if (work.cancel)
        _engine.cancel();
    if (work.search) {
        work.search->options = puzzleOptions();
        _engine.search(std::move(*work.search));
    }
}

void PuzzleScene::pollEngine() {
    SearchResult result;
    bool answered = false;
    while (_engine.poll(result)) {
        if (result.tag > 0) {  // one of the session's
            _session.answer(result);
            answered = true;
            continue;
        }
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

        case Question::None:
            break;
        }
    }
    if (answered && _page == Page::Solving)
        sessionChanged();
}

// --- the editor --------------------------------------------------------------------

void PuzzleScene::openEditor(const Setup& setup) {
    // Whatever either page had asked is about something that is gone.
    _session.close();
    _session.toEngine();
    _engine.cancel();
    _question = Question::None;
    _tag      = 0;
    _page     = Page::Editor;
    _setup    = setup;
    _brush    = {};
    _input.reset();
    cancelPress();
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
    _probeIn = position && position->hasLegalMoves() && _engine.usable() ? kProbeDelay : -1.0f;
    eventlog::note("setup " + _setup.fen());
    refreshEditor();
    keepAwake();
}

void PuzzleScene::probe() {
    _probeIn = -1.0f;
    const auto position = setupPosition();
    if (_page != Page::Editor || !position || !position->hasLegalMoves() || !_engine.usable())
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
    } else if (!_engine.usable()) {
        _panel->setStatus(text::kPossible, text::kNoEngineHere, Tone::Good);
    } else if (!_probeDone) {
        _panel->setStatus(text::kPossible, text::kStudying, Tone::Good);
    } else {
        std::string detail = text::finding(position->sideToMove(), _probeMate, _probeCp);
        if (!_probeMate || *_probeMate < 0)
            detail += std::string("\n") + text::kNoForcedMate;
        _panel->setStatus(text::kPossible, detail, Tone::Good);
    }

    const bool studied = playable && (_probeDone || !_engine.usable());
    _panel->solve->setEnabled(studied && _engine.usable());
    _panel->saveMine->setEnabled(studied);
    _panel->playFrom->setEnabled(playable);

    refreshMarks();
}

void PuzzleScene::copyFen() {
    desktop::copy(_setup.fen());
    flash(_panel->copyFen, text::kCopied, [this] { _panel->copyFen->setText(text::kCopyFen); });
}

void PuzzleScene::pasteFen() {
    // Whatever is on the clipboard, with the spaces at either end taken off.
    std::string clip = desktop::paste();
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
    // A question the editor had out is about the position being left.
    if (_question != Question::None) {
        _engine.cancel();
        _question = Question::None;
        _tag      = 0;
    }

    _session.setEngineUsable(_engine.usable());
    std::string error;
    if (!_session.open(puzzle.fen, puzzle.mateIn, &error)) {
        AXLOGW("puzzle {} is not a legal position: {}", puzzle.id, error);
        openEditor(Setup::fromFen(puzzle.fen).value_or(Setup::initial()));
        return;
    }

    _page   = Page::Solving;
    _puzzle = puzzle;
    _brush  = {};
    _top->setVisible(false);
    _bottom->setVisible(false);
    _caption->setVisible(true);
    _panel->setPage(PuzzlePanel::Page::Solving);
    _panel->setMine(puzzle.mine);
    _panel->setPuzzle(puzzle.title, puzzle.note, text::goal(_session.attacker(), puzzle.mateIn));
    sessionChanged();  // the board, set up by the Opened event
}

void PuzzleScene::restart() {
    const Puzzle puzzle = _puzzle;
    openPuzzle(puzzle);
}

void PuzzleScene::takeBack() {
    if (_page != Page::Solving)
        return;
    _session.takeBack();
    sessionChanged();
}

void PuzzleScene::askHint() {
    if (_page != Page::Solving)
        return;
    _session.hint();
    sessionChanged();
}

void PuzzleScene::showSolution() {
    if (_page != Page::Solving)
        return;
    _session.showSolution();
    sessionChanged();
}

void PuzzleScene::sessionChanged() {
    talkToEngine();

    using Kind      = PuzzleSession::Event::Kind;
    bool redraw     = false;  // the position on the board has changed
    bool moving     = false;  // ...by the last move, which is to be seen happening
    bool slide      = true;
    for (const PuzzleSession::Event& event : _session.takeEvents()) {
        switch (event.kind) {
        case Kind::Opened:
            eventlog::note("puzzle open " + (_puzzle.id.empty() ? _puzzle.fen : _puzzle.id) + " mate " +
                           std::to_string(_session.mateIn()));
            // The solving side sits at the bottom, and nothing is in hand.
            _input.reset();
            cancelPress();
            _board->setFlipped(_session.attacker() == Color::Black);
            layout();
            redraw = true;
            moving = false;
            break;
        case Kind::Moved:
            eventlog::note("puzzle move " + event.move + (event.bySolution ? " solution" : " player"));
            _input.reset();
            redraw = true;
            moving = true;
            slide  = event.slide;
            break;
        case Kind::Replied:
            eventlog::note("puzzle reply " + event.move);
            redraw = true;
            moving = true;
            slide  = true;
            break;
        case Kind::Wrong:
            eventlog::note("puzzle wrong");
            break;
        case Kind::TakenBack:
            eventlog::note(event.automatic ? "puzzle takeback wrong" : "puzzle takeback");
            _input.reset();
            redraw = true;
            moving = false;
            break;
        case Kind::Finished:
            eventlog::note(std::string("puzzle finished ") + (event.byPlayer ? "player" : "solution") +
                           (event.helped ? " helped" : ""));
            if (event.byPlayer && !event.helped && !_puzzle.id.empty())
                puzzles::markSolved(_puzzle.id);
            break;
        }
    }

    if (redraw) {
        const Game& game = _session.game();
        if (moving)
            _board->show(game.position(), game.lastMove(), slide);
        else
            _board->show(game.position());
    }
    refreshSolving();
    keepAwake();
}

std::string PuzzleScene::noteText(const PuzzleSession::Note& note) const {
    using Kind = PuzzleSession::Note::Kind;
    const std::string move = text::localSan(note.move);
    switch (note.kind) {
    case Kind::None:           return {};
    case Kind::Right:          return text::rightMove(move);
    case Kind::NoMate:         return text::wrongMove(move, note.movesLeft);
    case Kind::BestRight:      return text::bestMoveVerdict(move, true, text::localSan(note.best));
    case Kind::BestWrong:      return text::bestMoveVerdict(move, false, text::localSan(note.best));
    case Kind::GameOver:       return text::verdict(Status{note.outcome, std::nullopt}, std::nullopt).headline + ".";
    case Kind::NoEngine:       return text::kNoEngineHere;
    case Kind::Hint:           return std::string(text::kHint) + ": " + move;
    case Kind::LookingForHint: return text::kHintLooking;
    case Kind::SolvedAlone:    return text::kSolvedAlone;
    case Kind::SolvedHelped:   return text::kSolvedHelped;
    case Kind::SolutionShown:  return text::kSolutionDone;
    }
    return {};
}

std::string PuzzleScene::movesText() const {
    std::string out;
    const auto& history = _session.game().history();
    const auto& marks   = _session.marks();
    for (std::size_t i = 0; i < history.size(); ++i) {
        const Position& before = history[i].before;
        if (before.sideToMove() == Color::White)
            out += (out.empty() ? "" : " ") + std::to_string(before.fullmoveNumber()) + ". ";
        else if (i == 0)
            out += std::to_string(before.fullmoveNumber()) + "... ";
        else
            out += ' ';
        out += text::localSan(history[i].san);
        if (i < marks.size())
            out += suffix(marks[i]);
    }
    return out;
}

void PuzzleScene::refreshSolving() {
    if (_page != Page::Solving)
        return;
    using Tone  = PuzzlePanel::Tone;
    using Phase = PuzzleSession::Phase;

    const bool        showing = _session.showingSolution();
    const std::string detail  = noteText(_session.note());
    _panel->setMoves(movesText());
    switch (_session.phase()) {
    case Phase::Preparing:
        _panel->setStatus(text::kFindingBest, "", Tone::Neutral);
        break;
    case Phase::Ready:
        if (showing)
            _panel->setStatus(text::kShowingSolution, detail, Tone::Neutral);
        else if (!_engine.usable())
            _panel->setStatus(text::kYourMove, text::kNoEngineHere, Tone::Warning);
        else
            _panel->setStatus(text::kYourMove, detail, Tone::Neutral);
        break;
    case Phase::Checking:
        _panel->setStatus(text::kChecking, "", Tone::Neutral);
        break;
    case Phase::Replying:
        _panel->setStatus(showing ? text::kShowingSolution : text::kRight, detail,
                          showing ? Tone::Neutral : Tone::Good);
        break;
    case Phase::Wrong:
        _panel->setStatus(text::kWrong, detail + " " + text::kTryAgain, Tone::Bad);
        break;
    case Phase::Solved:
        _panel->setStatus(_session.helped() ? text::kSolution : text::kSolved, detail,
                          _session.helped() ? Tone::Neutral : Tone::Good);
        break;
    }

    const Game& game = _session.game();
    const bool  busy = _session.busy();
    _panel->hint->setEnabled(playerToMove() && _engine.usable());
    _panel->solution->setEnabled(_engine.usable() && !busy && !showing);
    _panel->undo->setEnabled(game.plies() > 0 && !busy && !showing);
    _panel->restart->setEnabled(game.plies() > 0 || _session.helped());

    _caption->setString(game.status().over() ? "" : text::toMoveCaption(game.position().sideToMove()));
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

void PuzzleScene::onMouseDown(const Vec2& point, bool right) {
    keepAwake();

    if (Dialog* dialog = modal()) {
        if (!right)
            dialog->mouseDown(point);
        return;
    }

    if (_input.promotionClick(point))
        return;

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

    if (_page == Page::Editor)
        editorDown(point);
    else if (!_input.mouseDown(point))
        _input.select(kNoSquare);  // off the board, or not the player's turn
}

void PuzzleScene::onMouseMove(const Vec2& point) {
    if (Dialog* dialog = modal()) {
        if (dialog->mouseMove(point))
            keepAwake();
        return;
    }
    _panel->buttons().mouseMove(point);
    const bool held = _page == Page::Editor ? editorMove(point) : _input.mouseMove(point);
    if (_panel->buttons().hoverChanged() || held)
        keepAwake();
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

    if (_page == Page::Editor)
        editorUp(point);
    else
        _input.mouseUp(point);
}

void PuzzleScene::editorDown(const Vec2& point) {
    _press = {};
    _press.point = point;

    const PieceTray::Item item = !_top->at(point).none() ? _top->at(point) : _bottom->at(point);
    if (!item.none()) {
        _press.down = true;
        _press.item = item;
        return;
    }
    const Square square = _board->squareAt(point);
    if (square != kNoSquare) {
        _press.down   = true;
        _press.square = square;
    }
}

bool PuzzleScene::editorMove(const Vec2& point) {
    if (!_press.down)
        return false;
    if (!_press.dragging && point.distance(_press.point) > BoardInput::kDragThreshold) {
        if (!_press.item.none()) {
            if (!_press.item.eraser) {
                // A piece lifted off the tray: a sprite of its own, over everything.
                _carried = look::sprite(_press.item.piece, _board->squareSize() * 1.12f);
                if (_carried != nullptr) {
                    addChild(_carried, 5);
                    _press.dragging = true;
                }
            }
        } else if (!_setup.at(_press.square).empty()) {
            _press.dragging = true;
            _board->beginDrag(_press.square, point);
        }
    }
    if (_press.dragging) {
        if (_carried != nullptr)
            _carried->setPosition(point);
        else
            _board->dragTo(point);
    }
    return true;
}

void PuzzleScene::editorUp(const Vec2& point) {
    if (!_press.down)
        return;
    const EditorPress press = _press;
    _press                  = {};

    if (!press.item.none()) {
        if (press.dragging) {
            dropFromTray(point, press.item);
        } else {
            // A click on the tray picks the brush, or puts it down again.
            _brush = _brush == press.item ? PieceTray::Item{} : press.item;
            _top->setChosen(_brush);
            _bottom->setChosen(_brush);
        }
        return;
    }

    const Square from = press.square;
    if (press.dragging) {
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
    if (_brush.none())
        return;
    // A click with a brush: put it there — or, if it is there already, take it off.
    if (_brush.eraser || _setup.at(from) == _brush.piece)
        _setup.remove(from);
    else
        place(from, _brush.piece);
    edited();
}

void PuzzleScene::cancelPress() {
    if (_carried != nullptr) {
        _carried->removeFromParent();
        _carried = nullptr;
    }
    _press = {};
    _input.cancelPress();
}

void PuzzleScene::dropFromTray(const Vec2& point, const PieceTray::Item& item) {
    if (_carried != nullptr) {
        _carried->removeFromParent();
        _carried = nullptr;
    }
    const Square target = _board->squareAt(point);
    if (target == kNoSquare || item.eraser)
        return;
    place(target, item.piece);
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
        _input.cancelPromotion();
        _brush = {};
        _top->setChosen(_brush);
        _bottom->setChosen(_brush);
        _input.select(kNoSquare);
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
    if (!_hooks.gameAtStake || !_hooks.gameAtStake())
        return;
    _confirm->askBeforeQuitting(closing);
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

    const Game& game = _session.game();
    if (const Move* last = game.lastMove()) {
        marks.lastFrom = last->from;
        marks.lastTo   = last->to;
    }
    const Position& position = game.position();
    if (position.inCheck())
        marks.check = position.kingSquare(position.sideToMove());

    using Sign = BoardNode::Sign;
    const Status status = game.status();
    if (status.outcome == Outcome::Checkmate && status.winner) {
        marks.kings[0] = {position.kingSquare(*status.winner), Sign::Won};
        marks.kings[1] = {position.kingSquare(opposite(*status.winner)), Sign::Mate};
    } else if (status.over()) {
        marks.kings[0] = {position.kingSquare(Color::White), Sign::Draw};
        marks.kings[1] = {position.kingSquare(Color::Black), Sign::Draw};
    } else if (position.inCheck()) {
        marks.kings[0] = {position.kingSquare(position.sideToMove()), Sign::Check};
    }

    if (const Square selected = _input.selected(); selected != kNoSquare && playerToMove()) {
        marks.selected = selected;
        marks.targets  = position.legalMovesFrom(selected);
    }
    marks.hintFrom = _session.hintFrom();
    marks.hintTo   = _session.hintTo();
    if (const auto& verdicts = _session.marks(); !verdicts.empty() && verdicts.size() == game.plies())
        marks.verdict = verdicts.back();
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

    // The session's own time: a defence about to be played, a wrong move
    // about to be taken back.
    _session.setEngineUsable(_engine.usable());
    if (_page == Page::Solving) {
        _session.tick(delta);
        if (_session.hasEvents())
            sessionChanged();
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

    // Everything that changes the screen with nobody touching anything; the
    // list has to be complete (see Wakefulness.h).
    _awake.update(delta, {
        {_engine.thinking(), "the engine is thinking"},
        {_probeIn >= 0.0f, "the position is about to be looked at"},
        {_session.timerRunning(), "a reply or a take-back is due"},
        {_board->animating(), "a piece is moving"},
        {_input.pressed() || _press.down, "the mouse button is down"},
        {_flashFor > 0.0f, "a notice is up"},
        {_engine.state() == Engine::State::Starting, "the engine is starting"},
        {!_opening.screenshot.empty(), "a screenshot is being taken"},
    });
}

void PuzzleScene::captureAndQuit() {
    desktop::screenshotWhenSettled(this, _opening.screenshot, [this] {
        return _engine.thinking() || _probeIn >= 0.0f || _session.timerRunning() || _board->animating() ||
               _engine.state() == Engine::State::Starting;
    });
}
