// A game: a starting position, the moves played from it, and how it stands.
//
// This is the part that remembers. A Position cannot tell that it has occurred
// three times, or what the fourth move was called; a Game can, and it can take
// a move back.
#pragma once

#include "chess/Position.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace chess {

enum class Outcome {
    Ongoing,
    Checkmate,
    Stalemate,
    FiftyMoves,            // fifty moves by each side with no capture and no pawn move
    Repetition,            // the same position for the third time
    InsufficientMaterial,  // nobody has enough left to mate
    Resignation,
};

struct Status {
    Outcome outcome = Outcome::Ongoing;
    // Set for Checkmate and Resignation; a draw has no winner.
    std::optional<Color> winner;

    bool over() const { return outcome != Outcome::Ongoing; }
    bool drawn() const { return over() && !winner.has_value(); }
};

class Game {
public:
    struct Record {
        Move        move;
        std::string san;     // as written in the position it was played in
        Position    before;  // that position
    };

    Game() : Game(Position::initial()) {}
    explicit Game(const Position& start);

    const Position& startPosition() const { return _start; }
    const Position& position() const { return _position; }
    const std::vector<Record>& history() const { return _history; }
    std::size_t plies() const { return _history.size(); }
    const Move* lastMove() const { return _history.empty() ? nullptr : &_history.back().move; }

    // Plays a move if it is legal and the game is not over. Only origin,
    // destination and promotion are read from the argument.
    bool play(const Move& move);
    bool playUci(std::string_view text);
    bool playSan(std::string_view text);

    // Takes back the last half-move. Also un-resigns.
    bool undo();

    // Somebody gives up: the named side, or the side to move.
    void resign(Color who);
    void resign() { resign(_position.sideToMove()); }

    Status status() const;

    // The game as a UCI `position` command, moves and all, so that the engine
    // sees the same history we do and can count repetitions itself.
    std::string uciPosition() const;

    // The pieces of `color` that have been captured so far, most valuable
    // first.
    std::vector<PieceType> capturedPieces(Color color) const;

    // Material on the board, white minus black, in pawns.
    int materialBalance() const;

private:
    Position            _start;
    Position            _position;
    std::vector<Record> _history;
    std::optional<Color> _resigned;
};

}  // namespace chess
