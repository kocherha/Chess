#include "chess/Opponent.h"

#include "chess/Pause.h"

#include <algorithm>
#include <utility>
#include <vector>

namespace chess {

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

Opponent::Opponent(std::uint32_t seed) : _rng(seed) {}

void Opponent::newGame(Color side, const Level& level) {
    withdraw();
    _side  = side;
    _level = level;
    _game  = Game();
    _answered.clear();
    _lost = false;
    _trouble.clear();
}

void Opponent::follow(const Game& game, bool engineUsable) {
    withdraw();  // whatever it was doing was about the position before
    _game = game;
    if (_lost || game.status().over() || game.position().sideToMove() != _side)
        return;
    if (!engineUsable) {
        lose("there is no engine to ask");
        return;
    }

    const std::string key = game.uciPosition();

    // Been here before in this game: the same answer as then.
    if (const auto known = _answered.find(key); known != _answered.end()) {
        _pause = Pause{known->second, kReplyPause};
        return;
    }

    // The handicap of the lowest levels: this move is not the engine's. It
    // takes a little longer, a little unevenly, as if thought about.
    std::uniform_real_distribution<float> chance(0.0f, 1.0f);
    if (chance(_rng) < static_cast<float>(_level.randomMoveShare)) {
        _pause = Pause{std::string(), kReplyPause + 0.35f * chance(_rng)};
        return;
    }

    SearchRequest request;
    request.position   = key;
    request.moveTimeMs = _level.moveTimeMs;
    request.tag = _tag = _nextTag++;
    _asked   = key;
    _request = std::move(request);
}

void Opponent::withdraw() {
    if (_tag != 0)
        _cancel = true;
    _tag = 0;
    _asked.clear();
    _request.reset();
    _pause.reset();
    _ready.reset();
}

Opponent::ToEngine Opponent::toEngine() {
    ToEngine work{_cancel, std::move(_request)};
    _cancel = false;
    _request.reset();
    return work;
}

void Opponent::answer(const SearchResult& result) {
    if (_tag == 0 || result.tag != _tag)
        return;  // an answer to a question that has since been withdrawn
    _tag = 0;

    // The engine was given this very game move by move, so a move that is
    // not legal here means the two disagree about the rules. Better to stop
    // than to guess.
    const auto move = _game.position().parseUci(result.bestMove);
    if (!move) {
        lose("its move '" + result.bestMove + "' is not legal in " + _game.position().fen());
        return;
    }
    _answered[_asked] = result.bestMove;
    _asked.clear();
    _ready = move;
}

void Opponent::tick(float seconds) {
    if (!_pause)
        return;
    _pause->left -= seconds;
    if (_pause->left > 0.0f)
        return;

    const Position& position = _game.position();
    std::optional<Move> move = _pause->move.empty() ? std::nullopt : position.parseUci(_pause->move);
    if (!move)
        move = randomMove(position, _rng);
    _pause.reset();
    if (!move)
        return;
    _answered[_game.uciPosition()] = move->uci();
    _ready = move;
}

std::optional<Move> Opponent::takeMove() {
    std::optional<Move> move = std::move(_ready);
    _ready.reset();
    return move;
}

void Opponent::engineFailed(const std::string& why) {
    if (!_lost)
        lose(why);
}

void Opponent::lose(std::string why) {
    withdraw();
    _lost    = true;
    _trouble = std::move(why);
}

}  // namespace chess
