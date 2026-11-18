#include "chess/BoardMarks.h"

namespace chess {

BoardMarks marksFor(const Game& game, const std::vector<Mark>& moveMarks) {
    BoardMarks marks;
    if (const Move* last = game.lastMove()) {
        marks.lastFrom = last->from;
        marks.lastTo   = last->to;
    }

    const Position& position = game.position();
    if (position.inCheck())
        marks.check = position.kingSquare(position.sideToMove());

    const auto sign = [&](Color king, GameSign what) {
        marks.kings[king == Color::White ? 0 : 1] = {position.kingSquare(king), what};
    };
    const Status status = game.status();
    switch (status.outcome) {
    case Outcome::Ongoing:
        if (position.inCheck())
            sign(position.sideToMove(), GameSign::Check);
        break;
    case Outcome::Checkmate:
    case Outcome::Resignation:
        if (status.winner) {
            sign(*status.winner, GameSign::Won);
            sign(opposite(*status.winner),
                 status.outcome == Outcome::Checkmate ? GameSign::Mate : GameSign::Resigned);
        }
        break;
    case Outcome::Stalemate:
    case Outcome::FiftyMoves:
    case Outcome::Repetition:
    case Outcome::InsufficientMaterial:
        sign(Color::White, GameSign::Draw);
        sign(Color::Black, GameSign::Draw);
        break;
    }

    if (!moveMarks.empty() && moveMarks.size() == game.plies())
        marks.verdict = moveMarks.back();
    return marks;
}

}  // namespace chess
