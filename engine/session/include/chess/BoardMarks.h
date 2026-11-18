// What the board shows on top of the pieces: which squares are lit, which
// badges stand where. A list of facts, drawn by BoardNode; and the one place
// that works out the part of it a game decides by itself (marksFor), the
// same for a game and for a problem.
#pragma once

#include "chess/Game.h"
#include "chess/Judge.h"

#include <array>
#include <vector>

namespace chess {

// How the game stands for one of the kings, as a badge on his square, in the
// signs a score sheet uses: "+" in check, "#" mated, "1" for the winner, "0"
// for the one who resigned, "½" for a draw of any kind.
enum class GameSign { None, Check, Mate, Won, Resigned, Draw };
struct KingSign {
    Square   square = kNoSquare;
    GameSign sign   = GameSign::None;

    bool operator==(const KingSign&) const = default;
};

struct BoardMarks {
    Square lastFrom = kNoSquare, lastTo = kNoSquare;
    Square selected = kNoSquare;
    Square check    = kNoSquare;  // the king that is in check
    Square hintFrom = kNoSquare, hintTo = kNoSquare;
    // What the last move was worth ("!!", "?", ...): a badge on the corner of
    // the square it went to. None for no badge.
    Mark verdict = Mark::None;
    // The badges on the kings, white's first. If the last move ended on a
    // king's square too, its own badge moves to the other corner.
    std::array<KingSign, 2> kings{};
    std::vector<Move> targets;  // where the selected piece may go
    // Squares the position editor points at: what makes the position
    // impossible. Tinted red under the pieces.
    std::vector<Square> problems;
};

// What `game` says about its own board: the last move, a king in check, the
// signs on the kings — and, from `moveMarks` (one per move, as far as they
// are known), the mark of the last move once every move has one. The rest —
// what is selected, a hint — is the screen's to add.
BoardMarks marksFor(const Game& game, const std::vector<Mark>& moveMarks);

}  // namespace chess
