// Marking a move the way an annotator would: "!!", "!", "?!", "?", "??".
//
// No engine says how good a move was. What it says is how good a position is,
// and a mark is what comes of comparing two of those: what the position was
// worth before the move — that is, with the best move played — and what it was
// worth once the move actually made was on the board. The difference is what
// the move threw away. This file is that comparison and nothing else: it is
// given the engine's numbers and never talks to an engine itself.
//
// The scale is not pawns. Dropping a pawn in a level game matters a great
// deal; dropping one when a queen up changes nothing. So scores are first
// turned into the chance of winning they stand for (the same curve the
// evaluation bar is drawn with), and it is that chance a move is judged by.
#pragma once

#include "chess/Position.h"

#include <cstdint>
#include <optional>
#include <string>

namespace chess {

enum class Mark : std::uint8_t {
    None,
    Brilliant,   // !!  the best move, and one that gives up material to be it
    Great,       // !   the only good move, and not one the position forced
    Inaccuracy,  // ?!  gave up a little
    Mistake,     // ?   gave up a lot
    Blunder,     // ??  gave up the game, or most of it
};

// "!!", "!", "?!", "?", "??"; empty for None.
const char* suffix(Mark mark);

// A score as a UCI engine reports it, for the side to move: hundredths of a
// pawn, or a forced mate in so many moves (negative: being mated). Mate 0 is
// the side to move already checkmated.
struct Score {
    std::optional<int> cp, mate;

    bool known() const { return cp.has_value() || mate.has_value(); }
    bool operator==(const Score&) const = default;
};

// The chance of winning a score stands for, from 0 (lost) through 0.5 (level)
// to 1 (won), for the side the score is for. The usual logistic curve: a pawn
// up is about 0.59, three pawns 0.75, five 0.86. An unknown score is 0.5.
double winChance(int centipawns);
double winChance(const Score& score);

// What the engine found, for judging one move.
struct Evidence {
    // The position before the move, for the side that then moved: its score
    // (which is the score of its best move), that move, and the score of the
    // second-best one if the engine was asked for it and there was one.
    Score                best;
    std::string          bestMove;  // UCI
    std::optional<Score> second;

    // The position after the move — for the side to move there, which is the
    // opponent. This is how an engine reports it; the sign is turned round here.
    Score after;
};

// How much of its winning chance the mover gave up, 0..1. Zero if the move was
// the engine's own first choice: two searches never agree to the last
// hundredth, and that noise is not the player's doing.
double loss(const Move& played, const Evidence& evidence);

// At and above these losses a move is marked ?!, ? and ??. They are the
// thresholds lichess uses (it states them on a scale twice as long).
inline constexpr double kInaccuracy = 0.05, kMistake = 0.10, kBlunder = 0.15;

// The mark for `played` in `before`. `previous` is the move that led to
// `before`, if there was one: taking back what was just taken is nobody's idea
// of a fine move, however far ahead of the alternatives it is.
//
// "!" is for a move that had to be found. Being the only good move is not
// enough: the only square for an attacked queen is the only good move too, and
// so is the one way out of a check. A move earns the mark when it stands well
// above every other and nothing obvious pointed to it — it does not simply win
// material, take back, answer a check, or save something that was about to be
// taken.
//
// "!!" is for a sacrifice that works: the move leaves material to be taken,
// is as good as the best there was, and the game was not won without it.
Mark judge(const Position& before, const Move& played, const Evidence& evidence,
           const Move* previous = nullptr);

}  // namespace chess
