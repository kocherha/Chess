#include "chess/PuzzleSession.h"

#include <algorithm>
#include <utility>

namespace chess {

namespace {

// What makes two questions to the engine the same question: everything in the
// request but its tag (the options are the caller's, and always the same).
std::string questionKey(const SearchRequest& request) {
    std::string key = request.position + "|" + std::to_string(request.mate) + "|" +
                      std::to_string(request.depth) + "|" + std::to_string(request.moveTimeMs);
    for (const std::string& move : request.searchMoves)
        key += " " + move;
    return key;
}

using Note  = PuzzleSession::Note;
using Event = PuzzleSession::Event;

Note makeNote(Note::Kind kind, std::string move = {}, std::string best = {}, int movesLeft = 0) {
    Note made;
    made.kind      = kind;
    made.move      = std::move(move);
    made.best      = std::move(best);
    made.movesLeft = movesLeft;
    return made;
}

Event makeEvent(Event::Kind kind, std::string move = {}) {
    Event made;
    made.kind = kind;
    made.move = std::move(move);
    return made;
}

}  // namespace

// --- setting up ------------------------------------------------------------------------

bool PuzzleSession::open(const std::string& fen, int mateIn, std::string* error) {
    const auto position = Position::fromFen(fen, error);
    if (!position)
        return false;

    dropQuestion();
    _open      = true;
    _fen       = fen;
    _mateIn    = mateIn;
    _game      = Game(*position);
    _attacker  = position->sideToMove();
    _attempt   = {};  // Ready

    // A best-move problem needs the best move before the player's can be
    // measured against it.
    if (_mateIn <= 0 && _engineUsable) {
        _attempt.phase = Phase::Preparing;
        SearchRequest request;
        request.position   = _game.uciPosition();
        request.depth      = kBestDepth;
        request.moveTimeMs = kBestMs;
        ask(Question::Prepare, std::move(request));
    }
    emit(makeEvent(Event::Kind::Opened));
    return true;
}

void PuzzleSession::restart() {
    if (_open)
        open(std::string(_fen), _mateIn);
}

void PuzzleSession::close() {
    dropQuestion();
    _open = false;
    _attempt.replyMove.clear();
    _attempt.wrongFor = 0.0f;
}

// --- what the player does ---------------------------------------------------------------

bool PuzzleSession::play(const Move& move, bool slide) {
    if (!playerToMove())
        return false;
    const auto legal = _game.position().findMove(move.from, move.to, move.promotion);
    if (!legal)
        return false;
    moved(*legal, slide);
    return true;
}

void PuzzleSession::takeBack() {
    if (!_open || _game.plies() == 0 || _attempt.phase == Phase::Preparing)
        return;
    dropQuestion();
    _attempt.autoSolve = false;
    _attempt.replyMove.clear();
    _attempt.wrongFor = 0.0f;

    do {
        _game.undo();
    } while (_game.plies() > 0 && _game.position().sideToMove() != _attacker);
    _attempt.marks.resize(_game.plies());
    _attempt.hintFrom = _attempt.hintTo = kNoSquare;
    _attempt.note  = {};
    _attempt.phase = Phase::Ready;

    emit(makeEvent(Event::Kind::TakenBack));
}

void PuzzleSession::hint() {
    if (!playerToMove() || !_engineUsable)
        return;
    _attempt.helped = true;

    // A best-move problem knows its answer already.
    if (_mateIn <= 0) {
        if (const auto move = _game.position().parseUci(_attempt.bestMove)) {
            _attempt.hintFrom = move->from;
            _attempt.hintTo   = move->to;
            _attempt.note     = makeNote(Note::Kind::Hint, _game.position().san(*move));
        }
        return;
    }

    askForTheMate();
    _attempt.note = makeNote(Note::Kind::LookingForHint);
}

void PuzzleSession::showSolution() {
    if (!_open || !_engineUsable || _attempt.phase == Phase::Preparing)
        return;
    if (_attempt.phase == Phase::Solved || _attempt.phase == Phase::Wrong)
        restart();  // so that the whole solution is seen

    // After restart(), which clears both.
    _attempt.autoSolve = true;
    _attempt.helped    = true;
    _attempt.hintFrom = _attempt.hintTo = kNoSquare;
    if (_attempt.phase == Phase::Preparing)
        return;  // a best-move problem: played as soon as the best move is known (Prepare)

    if (_attempt.phase != Phase::Ready || _game.position().sideToMove() != _attacker)
        return;  // the defence is on its way; the solution goes on after it (tick)
    if (_mateIn <= 0) {
        if (const auto move = _game.position().parseUci(_attempt.bestMove)) {
            moved(*move, true);
            return;
        }
        _attempt.autoSolve = false;  // the engine never named a best move here: nothing to show
        return;
    }
    askForTheMate();
}

// --- the engine -------------------------------------------------------------------------

PuzzleSession::ToEngine PuzzleSession::toEngine() {
    ToEngine work;
    work.cancel = std::exchange(_cancel, false);
    work.search = std::exchange(_request, std::nullopt);
    work.known  = std::exchange(_known, std::nullopt);
    return work;
}

void PuzzleSession::ask(Question question, SearchRequest request) {
    request.tag = _tag = _nextTag++;
    _question       = question;
    _asked          = questionKey(request);
    if (const auto known = _answers.find(_asked); known != _answers.end()) {
        _known      = known->second;
        _known->tag = _tag;
        _request.reset();  // the engine never hears of it,
        _cancel = true;    // and stops whatever it was asked before
        return;
    }
    _known.reset();
    _request = std::move(request);  // replaces one not yet sent: the engine never hears of it
}

void PuzzleSession::askForTheMate() {
    SearchRequest request;
    request.position   = _game.uciPosition();
    request.mate       = std::max(1, movesLeft());
    request.moveTimeMs = kHintMs;
    ask(Question::Hint, std::move(request));
}

void PuzzleSession::askForTheDefence() {
    SearchRequest request;
    request.position   = _game.uciPosition();
    request.moveTimeMs = kReplyMs;
    request.depth      = 2 * movesLeft() + 10;
    ask(Question::Reply, std::move(request));
    _attempt.phase = Phase::Replying;
}

void PuzzleSession::dropQuestion() {
    if (_question != Question::None)
        _cancel = true;
    _question = Question::None;
    _tag      = 0;
    _request.reset();
    _known.reset();
}

void PuzzleSession::answer(const SearchResult& result) {
    if (_question == Question::None || result.tag != _tag)
        return;  // to a question nobody is asking any more
    _answers[_asked] = result;
    const Question question = _question;
    _question = Question::None;
    _tag      = 0;

    switch (question) {
    case Question::Prepare:
        _attempt.bestMove  = result.bestMove;
        _attempt.bestScore = Score{result.scoreCp, result.mateIn};
        _attempt.phase     = Phase::Ready;
        // "Show the solution" was asked for while the engine was still
        // looking: now it has the move, play it.
        if (_attempt.autoSolve) {
            if (const auto move = _game.position().parseUci(_attempt.bestMove)) {
                moved(*move, true);
                return;
            }
            _attempt.autoSolve = false;  // nothing to show: the player's move again
        }
        return;

    case Question::Check:
        if (_attempt.phase != Phase::Checking)
            return;
        if (_mateIn <= 0)
            judgeBest(result);
        else
            judgeMate(result);
        return;

    case Question::Recheck: {
        if (_attempt.phase != Phase::Checking)
            return;
        // The defence's view: is it mated within the moves that are left?
        if (result.mateIn && *result.mateIn < 0 && -*result.mateIn <= movesLeft()) {
            _attempt.note = makeNote(Note::Kind::Right, lastSan());
            if (attackerMoves() == 1)
                _attempt.marks.back() = Mark::Great;
            reply(result.bestMove);
        } else {
            wrong(makeNote(Note::Kind::NoMate, lastSan(), {}, movesLeft() + 1));
        }
        return;
    }

    case Question::Reply:
        if (_attempt.phase == Phase::Replying)
            reply(result.bestMove);
        return;

    case Question::Hint: {
        if (_attempt.phase != Phase::Ready || _game.position().sideToMove() != _attacker)
            return;
        const auto move = _game.position().parseUci(result.bestMove);
        if (!move)
            return;
        if (_attempt.autoSolve) {
            moved(*move, true);
            return;
        }
        _attempt.hintFrom = move->from;
        _attempt.hintTo   = move->to;
        _attempt.note     = makeNote(Note::Kind::Hint, _game.position().san(*move));
        return;
    }

    case Question::None:
        return;
    }
}

// --- time -------------------------------------------------------------------------------

void PuzzleSession::tick(float seconds) {
    // The defence, after a pause, as if thought about.
    if (!_attempt.replyMove.empty()) {
        _attempt.replyDelay -= seconds;
        if (_attempt.replyDelay <= 0.0f) {
            const std::string uci = std::exchange(_attempt.replyMove, std::string());
            if (const auto move = _game.position().parseUci(uci); move && _attempt.phase == Phase::Replying) {
                _game.play(*move);
                emit(makeEvent(Event::Kind::Replied, uci));
                _attempt.marks.resize(_game.plies(), Mark::None);
                _attempt.phase = Phase::Ready;
                if (_attempt.autoSolve) {
                    askForTheMate();
                }
            }
        }
    }

    // A wrong move, taken back after it has been seen.
    if (_attempt.phase == Phase::Wrong && _attempt.wrongFor > 0.0f) {
        _attempt.wrongFor -= seconds;
        if (_attempt.wrongFor <= 0.0f) {
            _attempt.wrongFor = 0.0f;
            _game.undo();
            _attempt.marks.resize(_game.plies());
            _attempt.phase = Phase::Ready;  // the note still says what was wrong with it
            Event back = makeEvent(Event::Kind::TakenBack);
            back.automatic = true;
            emit(std::move(back));
        }
    }
}

std::vector<PuzzleSession::Event> PuzzleSession::takeEvents() {
    return std::exchange(_events, {});
}

// --- questions about the state ----------------------------------------------------------

int PuzzleSession::attackerMoves() const {
    int count = 0;
    for (const Game::Record& record : _game.history())
        count += record.before.sideToMove() == _attacker ? 1 : 0;
    return count;
}

int PuzzleSession::movesLeft() const {
    return _mateIn - attackerMoves();
}

bool PuzzleSession::playerToMove() const {
    return _open && _attempt.phase == Phase::Ready && !_attempt.autoSolve &&
           _game.position().sideToMove() == _attacker && !_game.status().over();
}

std::string PuzzleSession::lastSan() const {
    return _game.history().empty() ? std::string() : _game.history().back().san;
}

// --- a move by the solving side ---------------------------------------------------------

// The player's, or the engine's when it is showing the solution, is on the
// board. Now: is it right?
void PuzzleSession::moved(const Move& move, bool slide) {
    const std::string before = _game.uciPosition();
    if (!_game.play(move))
        return;

    Event made       = makeEvent(Event::Kind::Moved, move.uci());
    made.slide       = slide;
    made.bySolution  = _attempt.autoSolve;
    emit(std::move(made));

    _attempt.checkMove = move.uci();
    _attempt.marks.resize(_game.plies() - 1, Mark::None);
    _attempt.marks.push_back(Mark::None);
    _attempt.hintFrom = _attempt.hintTo = kNoSquare;

    const Status      status = _game.status();
    const std::string said   = lastSan();

    if (_mateIn > 0) {
        if (status.outcome == Outcome::Checkmate) {
            finish(!_attempt.autoSolve);
            return;
        }
        if (status.over()) {
            Note why    = makeNote(Note::Kind::GameOver, said);
            why.outcome = status.outcome;
            wrong(std::move(why));
            return;
        }
        if (movesLeft() <= 0) {
            wrong(makeNote(Note::Kind::NoMate, said, {}, 1));
            return;
        }
        if (!_engineUsable) {
            wrong(makeNote(Note::Kind::NoEngine));
            return;
        }
        if (_attempt.autoSolve) {
            // The engine's own move: no need to ask whether it is right.
            _attempt.note = makeNote(Note::Kind::Right, said);
            askForTheDefence();
            return;
        }
        // Is there still a mate in the moves that are left, by this move?
        SearchRequest check;
        check.position    = before;
        check.mate        = movesLeft() + 1;
        check.moveTimeMs  = kCheckMs;
        check.searchMoves = {_attempt.checkMove};
        ask(Question::Check, std::move(check));
        _attempt.phase = Phase::Checking;
        return;
    }

    // The best move: one move and done.
    if (_attempt.autoSolve || !_engineUsable) {
        _attempt.marks.back() = Mark::Great;
        _attempt.note         = makeNote(Note::Kind::BestRight, said);
        finish(false);
        return;
    }
    SearchRequest check;
    check.position    = before;
    check.depth       = kBestDepth;
    check.moveTimeMs  = kBestMs;
    check.searchMoves = {_attempt.checkMove};
    ask(Question::Check, std::move(check));
    _attempt.phase = Phase::Checking;
}

// The engine's verdict on a move in a mate problem, from the attacking side:
// a mate in the moves that are left, by that move?
void PuzzleSession::judgeMate(const SearchResult& result) {
    const std::string said = lastSan();
    if (result.mateIn && *result.mateIn > 0 && *result.mateIn <= movesLeft() + 1) {
        askForTheDefence();
        _attempt.note = makeNote(Note::Kind::Right, said);
        if (attackerMoves() == 1)
            _attempt.marks.back() = Mark::Great;
        return;
    }
    // Not found from the attacking side. Before calling it wrong, ask the
    // defence: is it mated within the moves that are left?
    SearchRequest recheck;
    recheck.position   = _game.uciPosition();
    recheck.moveTimeMs = kRecheckMs;
    recheck.depth      = 2 * movesLeft() + 12;
    ask(Question::Recheck, std::move(recheck));
}

// A best-move problem: as good as the best, or near enough.
void PuzzleSession::judgeBest(const SearchResult& result) {
    const std::string said = lastSan();
    const Score mine{result.scoreCp, result.mateIn};
    const bool  same  = _attempt.checkMove == _attempt.bestMove;
    const bool  right = same || !_attempt.bestScore.known() ||
                       (mine.known() && winChance(mine) >= winChance(_attempt.bestScore) - kBestTolerance);

    const Position& before = _game.history().back().before;
    const auto      best   = before.parseUci(_attempt.bestMove);
    const std::string bestSan = best ? before.san(*best) : std::string();
    if (right) {
        _attempt.marks.back() = Mark::Great;
        _attempt.note         = makeNote(Note::Kind::BestRight, said, bestSan);
        finish(true);
    } else {
        wrong(makeNote(Note::Kind::BestWrong, said, bestSan));
    }
}

void PuzzleSession::reply(const std::string& uci) {
    // No move, or none that can be played here: the engine is only asked for
    // a defence while the defending side has one (mate and stalemate end the
    // problem first, in moved()), so this does not happen — and if it ever
    // did, the board is left to the player rather than waiting for ever.
    if (!_game.position().parseUci(uci)) {
        _attempt.phase = Phase::Ready;
        return;
    }
    _attempt.replyMove  = uci;
    _attempt.replyDelay = kReplyPause;
    _attempt.phase      = Phase::Replying;
}

void PuzzleSession::wrong(Note why) {
    dropQuestion();
    if (!_attempt.marks.empty())
        _attempt.marks.back() = Mark::Mistake;
    emit(makeEvent(Event::Kind::Wrong));
    _attempt.phase    = Phase::Wrong;
    _attempt.wrongFor = kWrongSeconds;
    _attempt.note     = std::move(why);
}

void PuzzleSession::finish(bool byPlayer) {
    Event made    = makeEvent(Event::Kind::Finished);
    made.byPlayer = byPlayer;
    made.helped   = _attempt.helped;
    emit(std::move(made));

    _attempt.phase = Phase::Solved;
    if (_mateIn > 0)
        _attempt.note = makeNote(!byPlayer ? Note::Kind::SolutionShown
                     : _attempt.helped ? Note::Kind::SolvedHelped
                               : Note::Kind::SolvedAlone);
    _attempt.autoSolve = false;
}

}  // namespace chess
