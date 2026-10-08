#include "chess/Judge.h"

#include <algorithm>
#include <cmath>

namespace chess {

namespace {

// A good move is one that is the engine's first choice, or as near to it as
// makes no difference.
constexpr double kAsGoodAsBest = 0.02;

// "!" — the best move stands this far above every other, and what it keeps is
// worth keeping: being the only way to lose slowly earns no mark.
constexpr double kOnlyMoveGap   = 0.10;
constexpr double kWorthKeeping  = 0.25;

// "!!" — material worth at least this much is left to be taken (a piece for a
// pawn, a rook for a knight)...
constexpr int kSacrifice = 2;
// ...the mover is no worse for it...
constexpr double kStillStanding = 0.45;
// ...and was not winning anyway by the next-best move: giving a rook back when
// a queen up is housekeeping.
constexpr double kAlreadyWon = 0.90;

// What the opponent can win at once in `position`, in pawns: the best of the
// captures open to it, each followed through to the end of the exchange.
int onOffer(const Position& position) {
    int most = 0;
    for (const Move& move : position.legalMoves()) {
        if (move.isCapture())
            most = std::max(most, position.exchange(move));
    }
    return most;
}

}  // namespace

const char* suffix(Mark mark) {
    switch (mark) {
    case Mark::Brilliant:  return "!!";
    case Mark::Great:      return "!";
    case Mark::Inaccuracy: return "?!";
    case Mark::Mistake:    return "?";
    case Mark::Blunder:    return "??";
    case Mark::None:       break;
    }
    return "";
}

double winChance(int centipawns) {
    return 1.0 / (1.0 + std::exp(-0.00368208 * static_cast<double>(centipawns)));
}

double winChance(const Score& score) {
    if (score.mate)
        return *score.mate > 0 ? 1.0 : 0.0;  // mate 0: the side to move is the one mated
    if (score.cp)
        return winChance(*score.cp);
    return 0.5;
}

double loss(const Move& played, const Evidence& evidence) {
    if (played.uci() == evidence.bestMove)
        return 0.0;
    const double before = winChance(evidence.best);
    const double after  = 1.0 - winChance(evidence.after);
    return std::max(0.0, before - after);
}

Mark judge(const Position& before, const Move& played, const Evidence& evidence, const Move* previous) {
    if (!evidence.best.known() || !evidence.after.known())
        return Mark::None;

    const double lost = loss(played, evidence);
    if (lost >= kBlunder)
        return Mark::Blunder;
    if (lost >= kMistake)
        return Mark::Mistake;
    if (lost >= kInaccuracy)
        return Mark::Inaccuracy;
    if (lost > kAsGoodAsBest)
        return Mark::None;

    // From here on the move is a good one. Whether it is more than that
    // depends on the board, so it has to be a move that can be made on it.
    const auto move = before.findMove(played.from, played.to, played.promotion);
    if (!move || before.legalMoves().size() < 2)
        return Mark::None;  // a forced move is not a found one

    const double kept       = 1.0 - winChance(evidence.after);
    const double nextBest   = evidence.second ? winChance(*evidence.second) : 0.0;
    const bool   firstChoice = played.uci() == evidence.bestMove;

    // What the opponent can take once the move is made.
    const int left = onOffer(before.after(*move));

    // A sacrifice: after the move the opponent can win more than the move
    // itself took. Promotions are left out — what stands on the last rank
    // afterwards is not what was risked getting there.
    if (move->promotion == PieceType::None && evidence.second) {
        const int offered = left - pieceValue(move->captured);
        if (offered >= kSacrifice && kept >= kStillStanding && nextBest <= kAlreadyWon)
            return Mark::Brilliant;
    }

    if (firstChoice && evidence.second) {
        const double worth = winChance(evidence.best);
        if (worth - nextBest < kOnlyMoveGap || worth < kWorthKeeping)
            return Mark::None;

        // The only good move — but was there anything to find? Not when the
        // position itself said what to do:
        //   a capture that simply wins material;
        const bool winsMaterial = move->isCapture() && before.exchange(*move) > 0;
        //   taking back what has just been taken;
        const bool takesBack = move->isCapture() && previous != nullptr && previous->isCapture() &&
                               previous->to == move->to;
        //   a check to be answered;
        const auto idle      = before.passed();
        const bool inCheck   = !idle.has_value();
        //   or something about to be taken, which the move saves.
        const int  threatened = idle ? onOffer(*idle) : 0;
        const bool rescues    = threatened > 0 && left < threatened;

        if (!winsMaterial && !takesBack && !inCheck && !rescues)
            return Mark::Great;
    }
    return Mark::None;
}

}  // namespace chess
