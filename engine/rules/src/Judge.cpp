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

}  // namespace

int onOffer(const Position& position) {
    int most = 0;
    for (const Move& move : position.legalMoves()) {
        if (move.isCapture())
            most = std::max(most, position.exchange(move));
    }
    return most;
}

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

Standing standingFor(const Score& score, Color toMove) {
    const int sign = toMove == Color::White ? 1 : -1;
    if (score.mate && *score.mate != 0)
        return {Standing::Kind::Mate, sign * *score.mate};
    if (score.cp)
        return {Standing::Kind::Centipawns, sign * *score.cp};
    return {};
}

double winChance(int centipawns) {
    // lichess's curve (its "winning chances", fitted to rated games), so that
    // the marks mean here what they mean there.
    constexpr double kSteepness = 0.00368208;
    return 1.0 / (1.0 + std::exp(-kSteepness * static_cast<double>(centipawns)));
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

namespace {

// A sacrifice that works: after the move the opponent can win more than the
// move itself took, the mover is no worse for it, and was not winning anyway.
// `left`: what the opponent can take once the move is made.
bool sacrifices(const Move& move, int left, const Evidence& evidence, const Lead& lead) {
    // Promotions are left out — what stands on the last rank afterwards is
    // not what was risked getting there. And without a second line there is
    // no telling whether the game was won without it.
    if (move.promotion != PieceType::None || !evidence.second)
        return false;
    if (left - pieceValue(move.captured) < kSacrifice)
        return false;
    const double kept     = 1.0 - winChance(evidence.after);
    const double nextBest = winChance(*evidence.second);
    if (kept < kStillStanding || nextBest > kAlreadyWon)
        return false;
    // The offer has to be this move's. If as much could already be taken
    // before the opponent's last move, it was made then, and declined.
    return lead.from == nullptr || left > onOffer(*lead.from);
}

// The only good move — but was there anything to find? Not when the position
// itself said what to do.
bool saidByThePosition(const Position& before, const Move& move, int left, const Lead& lead) {
    // A capture that simply wins material;
    if (move.isCapture() && before.exchange(move) > 0)
        return true;
    // taking back what has just been taken;
    if (move.isCapture() && lead.move != nullptr && lead.move->isCapture() && lead.move->to == move.to)
        return true;
    // a check to be answered;
    const auto idle = before.passed();
    if (!idle)
        return true;
    // or something about to be taken, which the move saves.
    const int threatened = onOffer(*idle);
    return threatened > 0 && left < threatened;
}

// The engine's first choice, far above every other, and worth making.
bool standsAlone(const Move& played, const Evidence& evidence) {
    if (played.uci() != evidence.bestMove || !evidence.second)
        return false;
    const double worth = winChance(evidence.best);
    return worth - winChance(*evidence.second) >= kOnlyMoveGap && worth >= kWorthKeeping;
}

}  // namespace

Mark judge(const Position& before, const Move& played, const Evidence& evidence, const Lead& lead) {
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

    // What the opponent can take once the move is made.
    const int left = onOffer(before.after(*move));

    if (sacrifices(*move, left, evidence, lead))
        return Mark::Brilliant;
    if (standsAlone(played, evidence) && !saidByThePosition(before, *move, left, lead))
        return Mark::Great;
    return Mark::None;
}

}  // namespace chess
