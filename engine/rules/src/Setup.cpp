#include "chess/Setup.h"

#include "Board.h"

#include <algorithm>
#include <bit>
#include <cctype>
#include <cstdlib>
#include <limits>

namespace chess {

namespace {

using namespace board;

// The rank a side's pieces start on, and its pawns.
constexpr int homeRank(Color color) { return color == Color::White ? 0 : 7; }
constexpr int pawnRank(Color color) { return color == Color::White ? 1 : 6; }

bool slider(PieceType type) {
    return type == PieceType::Bishop || type == PieceType::Rook || type == PieceType::Queen;
}

// Is `between` on the straight line from `from` to `to`, strictly inside it?
bool strictlyBetween(Square from, Square to, Square between) {
    const int df = fileOf(to) - fileOf(from), dr = rankOf(to) - rankOf(from);
    if (!(df == 0 || dr == 0 || std::abs(df) == std::abs(dr)))
        return false;
    const int sf = (df > 0) - (df < 0), sr = (dr > 0) - (dr < 0);
    int f = fileOf(from) + sf, r = rankOf(from) + sr;
    while (makeSquare(f, r) != to) {
        if (makeSquare(f, r) == between)
            return true;
        f += sf;
        r += sr;
    }
    return false;
}

}  // namespace

// --- making one ------------------------------------------------------------------

Setup Setup::initial() {
    return from(Position::initial());
}

Setup Setup::from(const Position& position) {
    Setup setup;
    for (Square square = 0; square < 64; ++square)
        setup._board[static_cast<std::size_t>(square)] = position.at(square);
    setup._turn = position.sideToMove();
    for (Color color : {Color::White, Color::Black})
        for (bool kingSide : {true, false})
            setup.setCastling(color, kingSide, position.canCastle(color, kingSide));
    setup._enPassant = position.enPassantSquare();
    setup._halfmove  = position.halfmoveClock();
    setup._fullmove  = position.fullmoveNumber();
    return setup;
}

std::optional<Setup> Setup::fromFen(std::string_view fen) {
    const auto fields = splitFields(fen);
    if (fields.empty())
        return std::nullopt;

    Setup setup;
    const auto put = [&setup](Square square, Piece piece) {
        setup._board[static_cast<std::size_t>(square)] = piece;
    };
    if (!readPlacement(fields[0], put))
        return std::nullopt;

    if (fields.size() > 1 && fields[1] == "b")
        setup._turn = Color::Black;
    if (fields.size() > 2) {
        for (char c : fields[2]) {
            switch (c) {
            case 'K': setup._castling |= castleBit(Color::White, true); break;
            case 'Q': setup._castling |= castleBit(Color::White, false); break;
            case 'k': setup._castling |= castleBit(Color::Black, true); break;
            case 'q': setup._castling |= castleBit(Color::Black, false); break;
            default:  break;
            }
        }
    }
    if (fields.size() > 3)
        setup._enPassant = parseSquare(fields[3]);
    if (fields.size() > 4)
        setup._halfmove = std::max(0, std::atoi(std::string(fields[4]).c_str()));
    if (fields.size() > 5)
        setup.setFullmoveNumber(std::atoi(std::string(fields[5]).c_str()));
    return setup;
}

std::string Setup::fen() const {
    std::string out;
    board::writePlacement(out, [this](Square square) { return at(square); });

    out += _turn == Color::White ? " w " : " b ";

    std::string rights;
    if (canCastle(Color::White, true))  rights += 'K';
    if (canCastle(Color::White, false)) rights += 'Q';
    if (canCastle(Color::Black, true))  rights += 'k';
    if (canCastle(Color::Black, false)) rights += 'q';
    out += rights.empty() ? "-" : rights;

    out += ' ';
    out += enPassant() == kNoSquare ? "-" : squareName(enPassant());
    out += ' ' + std::to_string(_halfmove) + ' ' + std::to_string(_fullmove);
    return out;
}

// --- editing ---------------------------------------------------------------------

void Setup::put(Square square, Piece piece) {
    if (square < 0 || square > 63)
        return;
    _board[static_cast<std::size_t>(square)] = piece;
}

void Setup::clear() {
    _board.fill(Piece{});
    _castling  = 0;
    _enPassant = kNoSquare;
}

void Setup::setSideToMove(Color color) {
    if (color != _turn)
        _enPassant = kNoSquare;  // it was the other side's
    _turn = color;
}

bool Setup::castlingPossible(Color color, bool kingSide) const {
    const int rank = homeRank(color);
    return at(makeSquare(4, rank)).is(PieceType::King, color) &&
           at(makeSquare(kingSide ? 7 : 0, rank)).is(PieceType::Rook, color) && count({PieceType::King, color}) == 1;
}

bool Setup::castlingWanted(Color color, bool kingSide) const {
    return (_castling & castleBit(color, kingSide)) != 0;
}

bool Setup::canCastle(Color color, bool kingSide) const {
    return castlingWanted(color, kingSide) && castlingPossible(color, kingSide);
}

void Setup::setCastling(Color color, bool kingSide, bool wanted) {
    if (wanted)
        _castling |= castleBit(color, kingSide);
    else
        _castling &= static_cast<std::uint8_t>(~castleBit(color, kingSide));
}

std::vector<Square> Setup::enPassantCandidates() const {
    // White to move: a black pawn on the fifth rank that came from the
    // seventh, both squares it crossed now empty, and a white pawn beside it.
    const Color them   = opposite(_turn);
    const int   onRank = _turn == Color::White ? 4 : 3;
    const int   behind = _turn == Color::White ? 5 : 2;
    const int   start  = pawnRank(them);

    std::vector<Square> squares;
    for (int file = 0; file < 8; ++file) {
        if (!at(makeSquare(file, onRank)).is(PieceType::Pawn, them) || !at(makeSquare(file, behind)).empty() ||
            !at(makeSquare(file, start)).empty())
            continue;
        const bool taker = (file > 0 && at(makeSquare(file - 1, onRank)).is(PieceType::Pawn, _turn)) ||
                           (file < 7 && at(makeSquare(file + 1, onRank)).is(PieceType::Pawn, _turn));
        if (taker)
            squares.push_back(makeSquare(file, behind));
    }
    return squares;
}

Square Setup::enPassant() const {
    if (_enPassant == kNoSquare)
        return kNoSquare;
    const auto squares = enPassantCandidates();
    return std::find(squares.begin(), squares.end(), _enPassant) != squares.end() ? _enPassant : kNoSquare;
}

int Setup::count(Piece piece) const {
    return static_cast<int>(std::count(_board.begin(), _board.end(), piece));
}

Square Setup::kingSquare(Color color) const {
    return findKing([this](Square square) { return at(square); }, color);
}

std::vector<Square> Setup::attackers(Square target, Color by) const {
    std::vector<Square> found;
    board::findAttackers([this](Square square) { return at(square); }, target, by, [&](Square square) {
        found.push_back(square);
        return false;  // and go on looking
    });
    return found;
}

// --- is it possible? -------------------------------------------------------------

namespace {

// The fewest captures the pawns of one side can have made to stand where they
// stand, and which of them must have changed file to get there. Each pawn came
// from a file of its own, and moved at most one file sideways — by a capture —
// for every rank it advanced. -1 when no assignment of files works at all.
struct PawnOrigins {
    int captures = 0;
    std::vector<Square> moved;  // pawns that are not on the file they started on
};

PawnOrigins pawnOrigins(const std::vector<Square>& pawns, Color color) {
    constexpr int kNone = std::numeric_limits<int>::max() / 2;
    const std::size_t count = pawns.size();
    PawnOrigins result;
    if (count == 0)
        return result;
    if (count > 8) {
        result.captures = -1;
        return result;
    }

    // best[mask]: the fewest captures for the first popcount(mask) pawns to
    // have come from the files in mask. via[mask]: the file the last of them
    // came from, to read the assignment back.
    std::array<int, 256> best;
    std::array<int, 256> via;
    best.fill(kNone);
    via.fill(-1);
    best[0] = 0;
    for (unsigned mask = 0; mask < 256; ++mask) {
        if (best[mask] >= kNone)
            continue;
        const std::size_t index = static_cast<std::size_t>(std::popcount(mask));
        if (index >= count)
            continue;
        const Square pawn  = pawns[index];
        const int advanced = color == Color::White ? rankOf(pawn) - 1 : 6 - rankOf(pawn);
        for (int origin = 0; origin < 8; ++origin) {
            if ((mask & (1u << origin)) != 0)
                continue;
            const int shift = std::abs(fileOf(pawn) - origin);
            if (shift > advanced)
                continue;
            const unsigned next = mask | (1u << origin);
            if (best[mask] + shift < best[next]) {
                best[next] = best[mask] + shift;
                via[next]  = origin;
            }
        }
    }

    unsigned bestMask = 0;
    int fewest        = kNone;
    for (unsigned mask = 0; mask < 256; ++mask)
        if (static_cast<std::size_t>(std::popcount(mask)) == count && best[mask] < fewest) {
            fewest   = best[mask];
            bestMask = mask;
        }
    if (fewest >= kNone) {
        result.captures = -1;
        result.moved    = pawns;
        return result;
    }

    result.captures = fewest;
    for (unsigned mask = bestMask; mask != 0;) {
        const int origin = via[mask];
        const std::size_t index = static_cast<std::size_t>(std::popcount(mask)) - 1;
        if (fileOf(pawns[index]) != origin)
            result.moved.push_back(pawns[index]);
        mask &= ~(1u << origin);
    }
    std::sort(result.moved.begin(), result.moved.end());
    return result;
}

std::size_t side(Color color) { return static_cast<std::size_t>(color); }

void add(std::vector<Issue>& found, Flaw flaw, Color color, std::vector<Square> squares = {}, int need = 0,
         int have = 0) {
    found.push_back(Issue{flaw, color, std::move(squares), need, have});
}

constexpr Color kColours[2] = {Color::White, Color::Black};

// Where each side's kings and pawns stand, and how many pieces it has.
struct Census {
    std::array<std::vector<Square>, 2> kings, pawns, edgePawns;  // pawns: not on an edge rank
    std::array<int, 2>                 pieces{};

    int pawnCount(Color color) const {
        return static_cast<int>(pawns[side(color)].size() + edgePawns[side(color)].size());
    }
};

Census takeCensus(const Setup& setup) {
    Census census;
    for (Square square = 0; square < 64; ++square) {
        const Piece piece = setup.at(square);
        if (piece.empty())
            continue;
        ++census.pieces[side(piece.color)];
        if (piece.type == PieceType::King)
            census.kings[side(piece.color)].push_back(square);
        if (piece.type == PieceType::Pawn) {
            if (rankOf(square) == 0 || rankOf(square) == 7)
                census.edgePawns[side(piece.color)].push_back(square);
            else
                census.pawns[side(piece.color)].push_back(square);
        }
    }
    return census;
}

void kingIssues(const Census& census, std::vector<Issue>& found) {
    for (Color color : kColours) {
        if (census.kings[side(color)].empty())
            add(found, Flaw::MissingKing, color);
        else if (census.kings[side(color)].size() > 1)
            add(found, Flaw::ExtraKing, color, census.kings[side(color)]);
    }
}

void edgePawnIssues(const Census& census, std::vector<Issue>& found) {
    for (Color color : kColours)
        if (!census.edgePawns[side(color)].empty())
            add(found, Flaw::PawnOnEdge, color, census.edgePawns[side(color)]);
}

// Every piece beyond the starting set is a pawn that was promoted, and a pawn
// that was promoted is no longer on the board.
void materialIssues(const Setup& setup, const Census& census, std::vector<Issue>& found) {
    for (Color color : kColours) {
        const int pawnCount = census.pawnCount(color);
        if (pawnCount > 8) {
            std::vector<Square> all = census.pawns[side(color)];
            all.insert(all.end(), census.edgePawns[side(color)].begin(), census.edgePawns[side(color)].end());
            std::sort(all.begin(), all.end());
            add(found, Flaw::TooManyPawns, color, all, pawnCount, 8);
            continue;
        }

        const auto beyond = [&](PieceType type, int original) {
            return std::max(0, setup.count(Piece{type, color}) - original);
        };
        int promoted = beyond(PieceType::Queen, 1) + beyond(PieceType::Rook, 2) + beyond(PieceType::Knight, 2);

        // Bishops by the colour of their squares: a side starts with one of
        // each. And a bishop that has never been able to leave its square —
        // the pawns either side of it have not moved — is either there or was
        // taken there, so any other bishop on its colour was promoted.
        const int home = homeRank(color), front = pawnRank(color);
        const Piece bishop{PieceType::Bishop, color}, pawn{PieceType::Pawn, color};
        for (const int file : {2, 5}) {
            const Square start = makeSquare(file, home);
            const bool   light = lightSquare(start);
            int onColour       = 0;
            for (Square square = 0; square < 64; ++square)
                if (setup.at(square) == bishop && lightSquare(square) == light)
                    ++onColour;
            const bool shut =
                setup.at(makeSquare(file - 1, front)) == pawn && setup.at(makeSquare(file + 1, front)) == pawn;
            promoted += shut ? onColour - (setup.at(start) == bishop ? 1 : 0) : std::max(0, onColour - 1);
        }

        if (promoted > 8 - pawnCount)
            add(found, Flaw::TooManyPieces, color, {}, promoted, 8 - pawnCount);
    }
}

// Pawns change file only by capturing, so where they stand says how many of
// the other side's pieces they must have taken at the very least.
void pawnCaptureIssues(const Census& census, std::vector<Issue>& found) {
    for (Color color : kColours) {
        if (census.pawnCount(color) > 8)
            continue;  // already said: too many pawns
        const PawnOrigins origins = pawnOrigins(census.pawns[side(color)], color);
        const int missing         = std::max(0, 16 - census.pieces[side(opposite(color))]);
        if (origins.captures < 0)
            add(found, Flaw::PawnCaptures, color, origins.moved, -1, missing);
        else if (origins.captures > missing)
            add(found, Flaw::PawnCaptures, color, origins.moved, origins.captures, missing);
    }
}

}  // namespace

std::vector<Issue> Setup::issues() const {
    const Census census = takeCensus(*this);
    std::vector<Issue> found;
    kingIssues(census, found);  // first: without the kings it is not a position yet
    edgePawnIssues(census, found);
    materialIssues(*this, census, found);
    pawnCaptureIssues(census, found);

    // The rest is about the two kings and needs exactly one of each.
    if (census.kings[0].size() == 1 && census.kings[1].size() == 1)
        checkIssues(census.kings[side(_turn)].front(), census.kings[side(opposite(_turn))].front(), found);
    return found;
}

// `mine`: the king of the side to move.
void Setup::checkIssues(Square mine, Square theirs, std::vector<Issue>& found) const {
    const bool touching = std::max(std::abs(fileOf(mine) - fileOf(theirs)), std::abs(rankOf(mine) - rankOf(theirs))) <= 1;
    if (touching)
        add(found, Flaw::KingsTouching, _turn, {std::min(mine, theirs), std::max(mine, theirs)});

    const auto notKing = [this](std::vector<Square> squares) {
        squares.erase(std::remove_if(squares.begin(), squares.end(),
                                     [this](Square square) { return at(square).type == PieceType::King; }),
                      squares.end());
        return squares;
    };

    // The side that has just moved cannot have left its king attacked.
    if (const auto attacking = notKing(attackers(theirs, _turn)); !attacking.empty()) {
        std::vector<Square> squares{theirs};
        squares.insert(squares.end(), attacking.begin(), attacking.end());
        add(found, Flaw::WaitingSideInCheck, opposite(_turn), squares);
    }

    // A check on the side to move was given by the move just played: by the
    // piece that moved, or by one it uncovered. A pawn or a knight cannot be
    // uncovered — nothing stands between it and the king — so at most one of
    // the checking pieces is one of those, and it cannot be a pawn that has
    // never left its starting rank. A move can give at most two checks.
    const auto checkers = notKing(attackers(mine, opposite(_turn)));
    if (checkers.size() > 2) {
        std::vector<Square> squares{mine};
        squares.insert(squares.end(), checkers.begin(), checkers.end());
        add(found, Flaw::TooManyCheckers, _turn, squares);
    } else if (!checkers.empty()) {
        std::vector<Square> direct;
        bool unmoved = false;
        for (Square square : checkers) {
            if (slider(at(square).type))
                continue;
            direct.push_back(square);
            unmoved |= at(square).type == PieceType::Pawn && rankOf(square) == pawnRank(opposite(_turn));
        }
        if (direct.size() > 1 || unmoved) {
            std::vector<Square> squares{mine};
            squares.insert(squares.end(), checkers.begin(), checkers.end());
            add(found, Flaw::ImpossibleCheck, _turn, squares);
        }

        // And if the move just played was a pawn's two-square advance — which
        // is what an en passant square says — that pawn gave the check, or
        // uncovered it by leaving its starting square.
        if (const Square passed = enPassant(); passed != kNoSquare) {
            const int    forward = _turn == Color::White ? -8 : 8;  // the way their pawn went
            const Square pawn    = passed + forward;
            const Square start   = passed - forward;
            for (Square square : checkers) {
                if (square == pawn || (slider(at(square).type) && strictlyBetween(square, mine, start)))
                    continue;
                add(found, Flaw::BadEnPassant, _turn, {passed, square});
                break;
            }
        }
    }
}

std::optional<Position> Setup::position() const {
    if (!issues().empty())
        return std::nullopt;
    return Position::fromFen(fen());
}

}  // namespace chess
