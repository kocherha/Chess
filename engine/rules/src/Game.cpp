#include "chess/Game.h"

#include <algorithm>

namespace chess {

Game::Game(const Position& start) : _start(start), _position(start) {}

bool Game::play(const Move& wanted) {
    if (status().over())
        return false;

    // Look the move up rather than trust the caller's copy: the generator's
    // version carries the description (capture, castle, en passant) that the
    // notation and the history are built from.
    const auto move = _position.findMove(wanted.from, wanted.to, wanted.promotion);
    if (!move)
        return false;

    Record record;
    record.move   = *move;
    record.san    = _position.san(*move);
    record.before = _position;
    _history.push_back(std::move(record));

    _position.apply(*move);
    return true;
}

bool Game::playUci(std::string_view text) {
    const auto move = _position.parseUci(text);
    return move && play(*move);
}

bool Game::playSan(std::string_view text) {
    const auto move = _position.parseSan(text);
    return move && play(*move);
}

bool Game::undo() {
    if (_resigned) {
        _resigned.reset();
        return true;
    }
    if (_history.empty())
        return false;
    _position = _history.back().before;
    _history.pop_back();
    return true;
}

void Game::resign(Color who) {
    if (!status().over())
        _resigned = who;
}

Status Game::status() const {
    if (_resigned)
        return {Outcome::Resignation, opposite(*_resigned)};

    if (!_position.hasLegalMoves()) {
        if (_position.inCheck())
            return {Outcome::Checkmate, opposite(_position.sideToMove())};
        return {Outcome::Stalemate, std::nullopt};
    }

    if (_position.insufficientMaterial())
        return {Outcome::InsufficientMaterial, std::nullopt};

    // The clock counts half-moves, so fifty moves each is a hundred.
    if (_position.halfmoveClock() >= 100)
        return {Outcome::FiftyMoves, std::nullopt};

    // Threefold repetition. Only positions since the last capture or pawn move
    // can repeat — those are irreversible — so that is as far back as the
    // search needs to go, and the halfmove clock says exactly how far that is.
    const std::string key = _position.repetitionKey();
    int seen = 1;
    const std::size_t reach = std::min<std::size_t>(_history.size(),
                                                   static_cast<std::size_t>(_position.halfmoveClock()));
    for (std::size_t back = 1; back <= reach; ++back) {
        const Position& earlier = _history[_history.size() - back].before;
        if (earlier.sideToMove() == _position.sideToMove() && earlier.repetitionKey() == key &&
            ++seen >= 3)
            return {Outcome::Repetition, std::nullopt};
    }

    return {};
}

std::string Game::uciPosition() const {
    std::string command = "position ";
    if (_start == Position::initial())
        command += "startpos";
    else
        command += "fen " + _start.fen();

    if (!_history.empty()) {
        command += " moves";
        for (const Record& record : _history)
            command += ' ' + record.move.uci();
    }
    return command;
}

std::string Game::result() const {
    const Status now = status();
    if (!now.over())
        return "*";
    if (!now.winner)
        return "1/2-1/2";
    return *now.winner == Color::White ? "1-0" : "0-1";
}

std::string Game::pgn(const PgnTags& tags, const std::vector<std::string>& marks) const {
    const auto tag = [](const char* name, const std::string& value) {
        std::string line = "[";
        line += name;
        line += " \"";
        // A quote or a backslash inside a value is written with a backslash
        // before it; an empty value is the format's "unknown".
        for (char c : value.empty() ? std::string("?") : value) {
            if (c == '"' || c == '\\')
                line += '\\';
            line += c;
        }
        line += "\"]\n";
        return line;
    };

    const std::string outcome = result();

    std::string out;
    out += tag("Event", tags.event);
    out += tag("Site", tags.site);
    out += tag("Date", tags.date.empty() ? "????.??.??" : tags.date);
    out += tag("Round", "-");
    out += tag("White", tags.white);
    out += tag("Black", tags.black);
    out += tag("Result", outcome);
    if (!(_start == Position::initial())) {
        out += tag("SetUp", "1");
        out += tag("FEN", _start.fen());
    }
    out += '\n';

    // The moves, as words: a number before each of white's, and before
    // black's too when it is the first thing written ("12... Nf6"). A number
    // stays on the same line as the move it counts.
    std::vector<std::string> words;
    for (std::size_t i = 0; i < _history.size(); ++i) {
        const Position& before = _history[i].before;
        const std::string number = std::to_string(before.fullmoveNumber());
        const std::string move   = _history[i].san + (i < marks.size() ? marks[i] : std::string());
        if (before.sideToMove() == Color::White)
            words.push_back(number + ". " + move);
        else if (i == 0)
            words.push_back(number + "... " + move);
        else
            words.push_back(move);
    }
    words.push_back(outcome);

    std::size_t column = 0;
    for (const std::string& word : words) {
        if (column > 0 && column + 1 + word.size() > 80) {
            out += '\n';
            column = 0;
        }
        if (column > 0) {
            out += ' ';
            ++column;
        }
        out += word;
        column += word.size();
    }
    out += '\n';
    return out;
}

std::vector<PieceType> Game::capturedPieces(Color color) const {
    std::vector<PieceType> pieces;
    for (const Record& record : _history) {
        if (record.move.isCapture() && record.before.sideToMove() != color)
            pieces.push_back(record.move.captured);
    }
    std::sort(pieces.begin(), pieces.end(), [](PieceType a, PieceType b) {
        return pieceValue(a) != pieceValue(b) ? pieceValue(a) > pieceValue(b) : a > b;
    });
    return pieces;
}

int Game::materialBalance() const {
    int balance = 0;
    for (Square square = 0; square < 64; ++square) {
        const Piece piece = _position.at(square);
        if (!piece.empty())
            balance += piece.color == Color::White ? pieceValue(piece.type) : -pieceValue(piece.type);
    }
    return balance;
}

}  // namespace chess
