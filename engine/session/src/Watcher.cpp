#include "chess/Watcher.h"

#include <utility>

namespace chess {

void Watcher::newGame() {
    if (_tag != 0)
        _cancel = true;
    _tag = 0;
    _asked.clear();
    _lastProgress.reset();
    _request.reset();
    _game = Game();
    _following = false;
    _review.clear();
    _boardDone = false;
    _boardBest.clear();
    _hintWanted = false;
    _standing.reset();
    _hint.reset();
    _marksChanged = true;
}

void Watcher::follow(const Game& game) {
    // What the search in progress had found is kept; the search is not.
    keepProgress();
    if (_tag != 0)
        _cancel = true;
    _tag = 0;
    _asked.clear();
    _lastProgress.reset();
    _request.reset();

    const std::string before = _following ? _review.now() : std::string();
    _game      = game;
    _following = true;
    _boardDone = false;
    _boardBest.clear();
    if (_review.follow(game))
        _marksChanged = true;
    // A hint is for the position it was asked in.
    if (_review.now() != before || game.status().over())
        _hintWanted = false;

    lookAtTheBoard();
}

void Watcher::setEngineUsable(bool usable) {
    if (usable == _usable)
        return;
    _usable = usable;
    if (_usable && _tag == 0 && !_boardDone)
        lookAtTheBoard();
}

Watcher::ToEngine Watcher::toEngine() {
    ToEngine work{_cancel, std::move(_request)};
    _cancel = false;
    _request.reset();
    return work;
}

void Watcher::progress(const SearchResult& result) {
    if (_tag == 0 || result.tag != _tag)
        return;
    _lastProgress = result;
    // While it is still looking at the position on the board: follow along.
    // The bar keeps the previous position's score until then — one move
    // rarely changes the picture much, and a bar that jumped back to the
    // middle after every move would be all flicker.
    if (aboutTheBoard() && result.depth >= kShowFromDepth)
        stand(standingFor(Score{result.scoreCp, result.mateIn}, _game.position().sideToMove()));
}

void Watcher::answer(const SearchResult& result) {
    if (_tag == 0 || result.tag != _tag)
        return;  // an answer to a question that has since been withdrawn
    const std::string key   = std::move(_asked);
    const bool        board = key == _review.now() && !_game.status().over();
    _tag = 0;
    _asked.clear();
    _lastProgress.reset();

    if (_review.note(key, result, true))
        _marksChanged = true;
    if (board)
        boardIsDone(result.bestMove, Score{result.scoreCp, result.mateIn});
    else
        reviewNext();
}

void Watcher::lend() {
    keepProgress();
    // The engine's new user cancels what is left of the search; nothing to
    // ask of it here.
    _tag = 0;
    _asked.clear();
    _lastProgress.reset();
    _request.reset();
    _cancel = false;
}

void Watcher::wantHint() {
    if (_hintWanted)
        return;
    _hintWanted = true;
    if (_boardDone)
        giveHint();
    else if (_tag == 0)
        lookAtTheBoard();
    // Otherwise the search out now is about the board: nothing else is ever
    // asked before the board is done (reviewNext), and the hint comes with
    // its answer.
}

std::optional<Standing> Watcher::takeStanding() {
    auto standing = std::move(_standing);
    _standing.reset();
    return standing;
}

bool Watcher::takeMarksChanged() {
    return std::exchange(_marksChanged, false);
}

std::optional<std::string> Watcher::takeHint() {
    auto hint = std::move(_hint);
    _hint.reset();
    return hint;
}

// --- what to look at ------------------------------------------------------------------

void Watcher::lookAtTheBoard() {
    if (!_following)
        return;

    // A finished game needs no opinion: the result is the evaluation. The
    // moves that led to it may still be waiting for theirs.
    const Status status = _game.status();
    if (status.over()) {
        stand({!status.winner                   ? Standing::Kind::Drawn
               : *status.winner == Color::White ? Standing::Kind::WhiteWon
                                                : Standing::Kind::BlackWon,
               0});
        reviewNext();
        return;
    }
    if (!_usable)
        return;

    // Been here before — a move was taken back? Then it is already known.
    if (const auto* known = _review.found(_review.now()); known != nullptr && known->complete) {
        boardIsDone(known->best, known->score);
        return;
    }

    SearchRequest request;
    request.position   = _review.now();
    request.moveTimeMs = kBoardMs;
    request.depth      = kBoardDepth;
    ask(std::move(request));
}

void Watcher::reviewNext() {
    if (!_usable || _tag != 0)
        return;
    if (!_boardDone && !_game.status().over())
        return;  // the board first: a hint may be waiting for it
    if (auto request = _review.nextToReview())
        ask(std::move(*request));
}

void Watcher::ask(SearchRequest request) {
    request.tag = _tag = _nextTag++;
    _asked = request.position;
    _lastProgress.reset();
    _request = std::move(request);
}

bool Watcher::aboutTheBoard() const {
    return _asked == _review.now() && !_game.status().over();
}

void Watcher::boardIsDone(const std::string& best, const Score& score) {
    _boardDone = true;
    _boardBest = best;
    stand(standingFor(score, _game.position().sideToMove()));
    if (_hintWanted)
        giveHint();
    reviewNext();
}

void Watcher::giveHint() {
    _hintWanted = false;
    if (_game.position().parseUci(_boardBest))
        _hint = _boardBest;
}

void Watcher::keepProgress() {
    if (_tag != 0 && _lastProgress && _review.note(_asked, *_lastProgress, false))
        _marksChanged = true;
}

void Watcher::stand(const Standing& standing) {
    if (standing.kind != Standing::Kind::Unknown)
        _standing = standing;
}

}  // namespace chess
