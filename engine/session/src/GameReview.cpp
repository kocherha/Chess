#include "chess/GameReview.h"

namespace chess {

namespace {

// What a move can be judged by, from a search's result: what the engine
// noted at the judging depth, both lines — or, from a search that ran its
// course without getting there, its last word if that is deep enough.
std::optional<GameReview::Judgement> judgement(const SearchResult& result, bool complete) {
    GameReview::Judgement out;
    // Whole: the second line is there too, or there is no second line at all
    // (one legal move). With the first line only, the second is still to come.
    if (result.noted && (result.noted->second || result.secondDepth == 0)) {
        out.depth = GameReview::kJudgeDepth;
        out.score = Score{result.noted->scoreCp, result.noted->mateIn};
        out.best  = result.noted->pvMove;
        if (result.noted->second)
            out.second = Score{result.noted->second->scoreCp, result.noted->second->mateIn};
        return out;
    }
    if (!complete || result.depth < GameReview::kMarkDepth)
        return std::nullopt;
    out.depth = result.depth;
    out.score = Score{result.scoreCp, result.mateIn};
    out.best  = !result.bestMove.empty() ? result.bestMove : result.pvMove;
    // The second line of an iteration arrives after the first; one from the
    // iteration before is close enough, anything older is not.
    if ((result.secondCp || result.secondMate) && result.secondDepth >= result.depth - 1)
        out.second = Score{result.secondCp, result.secondMate};
    return out;
}

}  // namespace

void GameReview::clear() {
    _findings.clear();
    _places.clear();
    follow(Game());
}

bool GameReview::follow(const Game& game) {
    _game = game;

    // The engine is asked about each position by its `position` command; what
    // it says is filed under the position itself, and how many times it has
    // stood on the board before in this game.
    _keys.clear();
    std::map<std::string, int> seen;
    const auto& history = _game.history();
    for (std::size_t ply = 0; ply <= history.size(); ++ply) {
        const Position& position = ply < history.size() ? history[ply].before : _game.position();
        std::string place        = position.repetitionKey();
        place += " #" + std::to_string(++seen[place]);
        _keys.push_back(_game.uciPosition(ply));
        _places[_keys.back()] = std::move(place);
    }
    return judgeMoves();
}

bool GameReview::note(const std::string& key, const SearchResult& result, bool complete) {
    const auto place = _places.find(key);
    if (place == _places.end() || (!result.scoreCp && !result.mateIn))
        return false;
    Finding& finding = _findings[place->second];

    // Its latest word, unless what is already known is better.
    if (complete || (!finding.complete && result.depth >= finding.depth)) {
        finding.depth    = result.depth;
        finding.complete = complete;
        finding.score    = Score{result.scoreCp, result.mateIn};
        finding.best     = !result.bestMove.empty() ? result.bestMove : result.pvMove;
    }
    if (finding.judged)
        return false;
    finding.judged = judgement(result, complete);
    return finding.judged && judgeMoves();
}

const GameReview::Finding* GameReview::found(const std::string& key) const {
    const auto place = _places.find(key);
    if (place == _places.end())
        return nullptr;
    const auto it = _findings.find(place->second);
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
        // Judged already; or looked at to the end without getting deep enough
        // to judge by, which another look would not change.
        const Finding* known = found(_keys[i]);
        if (known != nullptr && (known->judged || known->complete))
            continue;

        SearchRequest request;
        request.position   = _keys[i];
        request.moveTimeMs = kReviewMs;
        request.depth      = kJudgeDepth;
        request.noteDepth  = kJudgeDepth;
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

    const auto judged = [this](std::size_t ply) -> const Judgement* {
        const Finding* finding = found(_keys[ply]);
        return finding != nullptr && finding->judged ? &*finding->judged : nullptr;
    };
    for (std::size_t i = 0; i < history.size() && i + 1 < _keys.size(); ++i) {
        const Judgement* before = judged(i);
        if (before == nullptr)
            continue;

        Evidence evidence;
        evidence.best     = before->score;
        evidence.bestMove = before->best;
        evidence.second   = before->second;
        if (i + 1 == history.size() && ending) {
            evidence.after = *ending;
        } else {
            const Judgement* after = judged(i + 1);
            if (after == nullptr)
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
