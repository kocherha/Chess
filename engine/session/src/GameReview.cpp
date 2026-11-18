#include "chess/GameReview.h"

namespace chess {

namespace {

// Deep enough to judge a move by?
bool usable(const GameReview::Finding* finding) {
    return finding != nullptr && finding->score.known() && finding->depth >= GameReview::kMarkDepth;
}

}  // namespace

void GameReview::clear() {
    _findings.clear();
    follow(Game());
}

bool GameReview::follow(const Game& game) {
    _game = game;

    _keys.clear();
    std::string key = "position ";
    if (_game.startPosition() == Position::initial())
        key += "startpos";
    else
        key += "fen " + _game.startPosition().fen();
    _keys.push_back(key);

    bool first = true;
    for (const Game::Record& record : _game.history()) {
        key += first ? " moves " : " ";
        key += record.move.uci();
        first = false;
        _keys.push_back(key);
    }
    return judgeMoves();
}

bool GameReview::note(const std::string& key, const SearchResult& result, bool complete) {
    if (key.empty() || (!result.scoreCp && !result.mateIn))
        return false;
    Finding& finding = _findings[key];
    if (!complete && (finding.complete || result.depth < finding.depth))
        return false;  // what is already known is better

    finding.depth    = result.depth;
    finding.complete = complete;
    finding.score    = Score{result.scoreCp, result.mateIn};
    finding.best     = !result.bestMove.empty() ? result.bestMove : result.pvMove;
    finding.second.reset();
    // The second line of an iteration arrives after the first; until it does,
    // the second-best move is from the iteration before, which is close enough.
    if ((result.secondCp || result.secondMate) && result.secondDepth >= result.depth - 1)
        finding.second = Score{result.secondCp, result.secondMate};
    return judgeMoves();
}

const GameReview::Finding* GameReview::found(const std::string& key) const {
    const auto it = _findings.find(key);
    return it == _findings.end() ? nullptr : &it->second;
}

std::optional<SearchRequest> GameReview::nextToReview() const {
    // The last position of a game that ended on the board is judged without
    // an engine (see judgeMoves); after a resignation it is a position like
    // any other.
    const Status status  = _game.status();
    const bool   settled = status.over() && status.outcome != Outcome::Resignation;

    for (std::size_t i = _keys.size(); i-- > 0;) {
        if (i + 1 == _keys.size() && settled)
            continue;
        const Finding* known = found(_keys[i]);
        if (known != nullptr && (known->complete || known->depth >= kReviewDepth))
            continue;

        SearchRequest request;
        request.position   = _keys[i];
        request.moveTimeMs = kReviewMs;
        request.depth      = kReviewDepth;
        return request;
    }
    return std::nullopt;
}

Mark GameReview::mark(std::size_t ply) const {
    return ply < _marks.size() ? _marks[ply] : Mark::None;
}

bool GameReview::judgeMoves() {
    const auto& history = _game.history();
    std::vector<Mark> marks(history.size(), Mark::None);

    // The last position of a game that ended on the board needs no engine: a
    // side that has been mated has no chances left, and a draw is a draw.
    std::optional<Score> ending;
    const Status status = _game.status();
    if (status.outcome == Outcome::Checkmate)
        ending = Score{std::nullopt, 0};
    else if (status.over() && status.outcome != Outcome::Resignation)
        ending = Score{0, std::nullopt};

    for (std::size_t i = 0; i < history.size() && i + 1 < _keys.size(); ++i) {
        const Finding* before = found(_keys[i]);
        if (!usable(before))
            continue;

        Evidence evidence;
        evidence.best     = before->score;
        evidence.bestMove = before->best;
        evidence.second   = before->second;
        if (i + 1 == history.size() && ending) {
            evidence.after = *ending;
        } else {
            const Finding* after = found(_keys[i + 1]);
            if (!usable(after))
                continue;
            evidence.after = after->score;
        }
        marks[i] = judge(history[i].before, history[i].move, evidence, i > 0 ? &history[i - 1].move : nullptr);
    }

    if (marks == _marks)
        return false;
    _marks = std::move(marks);
    return true;
}

}  // namespace chess
