#include "chess/Position.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <utility>

namespace chess {

namespace {

constexpr std::uint8_t kWhiteKing = 1, kWhiteQueen = 2, kBlackKing = 4, kBlackQueen = 8;

constexpr Square kA1 = 0, kE1 = 4, kH1 = 7, kA8 = 56, kE8 = 60, kH8 = 63;

struct Step {
    int file, rank;
};
constexpr Step kKnightSteps[] = {{1, 2}, {2, 1}, {2, -1}, {1, -2}, {-1, -2}, {-2, -1}, {-2, 1}, {-1, 2}};
constexpr Step kKingSteps[]   = {{1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}, {0, -1}, {1, -1}};
constexpr Step kDiagonals[]   = {{1, 1}, {-1, 1}, {-1, -1}, {1, -1}};
constexpr Step kStraights[]   = {{1, 0}, {0, 1}, {-1, 0}, {0, -1}};

constexpr std::uint8_t castleBit(Color color, bool kingSide) {
    return color == Color::White ? (kingSide ? kWhiteKing : kWhiteQueen)
                                 : (kingSide ? kBlackKing : kBlackQueen);
}

PieceType typeFromLetter(char letter) {
    switch (std::tolower(static_cast<unsigned char>(letter))) {
    case 'p': return PieceType::Pawn;
    case 'n': return PieceType::Knight;
    case 'b': return PieceType::Bishop;
    case 'r': return PieceType::Rook;
    case 'q': return PieceType::Queen;
    case 'k': return PieceType::King;
    default:  return PieceType::None;
    }
}

bool fail(std::string* error, std::string message) {
    if (error != nullptr)
        *error = std::move(message);
    return false;
}

std::vector<std::string_view> splitFields(std::string_view text) {
    std::vector<std::string_view> fields;
    std::size_t i = 0;
    while (i < text.size()) {
        while (i < text.size() && std::isspace(static_cast<unsigned char>(text[i])))
            ++i;
        std::size_t start = i;
        while (i < text.size() && !std::isspace(static_cast<unsigned char>(text[i])))
            ++i;
        if (i > start)
            fields.push_back(text.substr(start, i - start));
    }
    return fields;
}

bool parseNumber(std::string_view text, int& out) {
    if (text.empty() || text.size() > 6)
        return false;
    int value = 0;
    for (char c : text) {
        if (c < '0' || c > '9')
            return false;
        value = value * 10 + (c - '0');
    }
    out = value;
    return true;
}

}  // namespace

// --- names ---------------------------------------------------------------------

std::string squareName(Square square) {
    if (square < 0 || square > 63)
        return "-";
    return {static_cast<char>('a' + fileOf(square)), static_cast<char>('1' + rankOf(square))};
}

Square parseSquare(std::string_view name) {
    if (name.size() != 2 || name[0] < 'a' || name[0] > 'h' || name[1] < '1' || name[1] > '8')
        return kNoSquare;
    return makeSquare(name[0] - 'a', name[1] - '1');
}

char pieceLetter(PieceType type) {
    switch (type) {
    case PieceType::Pawn:   return 'P';
    case PieceType::Knight: return 'N';
    case PieceType::Bishop: return 'B';
    case PieceType::Rook:   return 'R';
    case PieceType::Queen:  return 'Q';
    case PieceType::King:   return 'K';
    default:                return '?';
    }
}

char pieceChar(Piece piece) {
    const char letter = pieceLetter(piece.type);
    return piece.color == Color::White
               ? letter
               : static_cast<char>(std::tolower(static_cast<unsigned char>(letter)));
}

int pieceValue(PieceType type) {
    switch (type) {
    case PieceType::Pawn:   return 1;
    case PieceType::Knight: return 3;
    case PieceType::Bishop: return 3;
    case PieceType::Rook:   return 5;
    case PieceType::Queen:  return 9;
    default:                return 0;
    }
}

std::string Move::uci() const {
    std::string text = squareName(from) + squareName(to);
    if (promotion != PieceType::None)
        text += static_cast<char>(std::tolower(static_cast<unsigned char>(pieceLetter(promotion))));
    return text;
}

// --- FEN -----------------------------------------------------------------------

Position Position::initial() {
    return *fromFen(kStartFen);
}

std::optional<Position> Position::fromFen(std::string_view fen, std::string* error) {
    const auto fields = splitFields(fen);
    if (fields.size() < 4) {
        fail(error, "a FEN needs at least four fields");
        return std::nullopt;
    }

    Position position;

    // Placement: ranks from the eighth down to the first, files a to h.
    int file = 0, rank = 7;
    int kings[2] = {0, 0};
    for (char c : fields[0]) {
        if (c == '/') {
            if (file != 8 || rank == 0) {
                fail(error, "malformed piece placement");
                return std::nullopt;
            }
            file = 0;
            --rank;
        } else if (c >= '1' && c <= '8') {
            file += c - '0';
        } else {
            const PieceType type = typeFromLetter(c);
            if (type == PieceType::None || file > 7) {
                fail(error, "malformed piece placement");
                return std::nullopt;
            }
            const Color color = std::isupper(static_cast<unsigned char>(c)) ? Color::White : Color::Black;
            if (type == PieceType::Pawn && (rank == 0 || rank == 7)) {
                fail(error, "a pawn cannot stand on the first or last rank");
                return std::nullopt;
            }
            if (type == PieceType::King)
                ++kings[static_cast<int>(color)];
            position._board[static_cast<std::size_t>(makeSquare(file, rank))] = Piece{type, color};
            ++file;
        }
        if (file > 8) {
            fail(error, "malformed piece placement");
            return std::nullopt;
        }
    }
    if (rank != 0 || file != 8) {
        fail(error, "malformed piece placement");
        return std::nullopt;
    }
    if (kings[0] != 1 || kings[1] != 1) {
        fail(error, "each side needs exactly one king");
        return std::nullopt;
    }

    if (fields[1] == "w")
        position._turn = Color::White;
    else if (fields[1] == "b")
        position._turn = Color::Black;
    else {
        fail(error, "the side to move must be w or b");
        return std::nullopt;
    }

    if (fields[2] != "-") {
        for (char c : fields[2]) {
            switch (c) {
            case 'K': position._castling |= kWhiteKing; break;
            case 'Q': position._castling |= kWhiteQueen; break;
            case 'k': position._castling |= kBlackKing; break;
            case 'q': position._castling |= kBlackQueen; break;
            default:
                fail(error, "malformed castling rights");
                return std::nullopt;
            }
        }
    }
    // A right is only worth keeping if the king and that rook are at home.
    const auto keepIf = [&](std::uint8_t bit, Square king, Square rook, Color color) {
        if (!position.at(king).is(PieceType::King, color) || !position.at(rook).is(PieceType::Rook, color))
            position._castling &= static_cast<std::uint8_t>(~bit);
    };
    keepIf(kWhiteKing, kE1, kH1, Color::White);
    keepIf(kWhiteQueen, kE1, kA1, Color::White);
    keepIf(kBlackKing, kE8, kH8, Color::Black);
    keepIf(kBlackQueen, kE8, kA8, Color::Black);

    if (fields[3] != "-") {
        const Square square = parseSquare(fields[3]);
        // The square behind a pawn that has just advanced two: rank 6 with
        // white to move, rank 3 with black to move.
        const int expected = position._turn == Color::White ? 5 : 2;
        if (square == kNoSquare || rankOf(square) != expected) {
            fail(error, "malformed en passant square");
            return std::nullopt;
        }
        const Square pawn = makeSquare(fileOf(square), position._turn == Color::White ? 4 : 3);
        if (position.at(pawn).is(PieceType::Pawn, opposite(position._turn)) && position.at(square).empty())
            position._enPassant = square;
    }

    if (fields.size() > 4 && !parseNumber(fields[4], position._halfmove)) {
        fail(error, "malformed halfmove clock");
        return std::nullopt;
    }
    if (fields.size() > 5 && (!parseNumber(fields[5], position._fullmove) || position._fullmove < 1)) {
        fail(error, "malformed move number");
        return std::nullopt;
    }

    // The side that has just moved cannot have left its own king attacked.
    if (position.isAttacked(position.kingSquare(opposite(position._turn)), position._turn)) {
        fail(error, "the side not to move is in check");
        return std::nullopt;
    }

    return position;
}

void Position::placementTo(std::string& out) const {
    for (int rank = 7; rank >= 0; --rank) {
        int gap = 0;
        for (int file = 0; file < 8; ++file) {
            const Piece piece = at(makeSquare(file, rank));
            if (piece.empty()) {
                ++gap;
                continue;
            }
            if (gap > 0) {
                out += static_cast<char>('0' + gap);
                gap = 0;
            }
            out += pieceChar(piece);
        }
        if (gap > 0)
            out += static_cast<char>('0' + gap);
        if (rank > 0)
            out += '/';
    }
}

std::string Position::fen() const {
    std::string out;
    placementTo(out);
    out += _turn == Color::White ? " w " : " b ";

    if (_castling == 0)
        out += '-';
    else {
        if (_castling & kWhiteKing)  out += 'K';
        if (_castling & kWhiteQueen) out += 'Q';
        if (_castling & kBlackKing)  out += 'k';
        if (_castling & kBlackQueen) out += 'q';
    }

    out += ' ';
    out += _enPassant == kNoSquare ? std::string("-") : squareName(_enPassant);
    out += ' ' + std::to_string(_halfmove) + ' ' + std::to_string(_fullmove);
    return out;
}

std::string Position::repetitionKey() const {
    std::string out;
    placementTo(out);
    out += _turn == Color::White ? " w " : " b ";
    out += static_cast<char>('a' + _castling);

    // FIDE counts the en passant right only when it can actually be used, so a
    // double pawn push that nobody could capture does not make the position
    // "different" from its later repetitions.
    if (_enPassant != kNoSquare) {
        const auto moves = legalMoves();
        const bool usable = std::any_of(moves.begin(), moves.end(),
                                        [](const Move& move) { return move.enPassant; });
        if (usable)
            out += ' ' + squareName(_enPassant);
    }
    return out;
}

// --- questions about the board -------------------------------------------------

bool Position::canCastle(Color color, bool kingSide) const {
    return (_castling & castleBit(color, kingSide)) != 0;
}

Square Position::kingSquare(Color color) const {
    for (Square square = 0; square < 64; ++square) {
        if (at(square).is(PieceType::King, color))
            return square;
    }
    return kNoSquare;
}

bool Position::isAttacked(Square square, Color by) const {
    if (square == kNoSquare)
        return false;
    const int file = fileOf(square), rank = rankOf(square);

    // A pawn attacks diagonally forward, so the attacker stands one rank
    // *behind* the square from its own point of view.
    const int pawnRank = rank + (by == Color::White ? -1 : 1);
    for (int df : {-1, 1}) {
        if (onBoard(file + df, pawnRank) && at(makeSquare(file + df, pawnRank)).is(PieceType::Pawn, by))
            return true;
    }

    for (const Step& step : kKnightSteps) {
        if (onBoard(file + step.file, rank + step.rank) &&
            at(makeSquare(file + step.file, rank + step.rank)).is(PieceType::Knight, by))
            return true;
    }

    for (const Step& step : kKingSteps) {
        if (onBoard(file + step.file, rank + step.rank) &&
            at(makeSquare(file + step.file, rank + step.rank)).is(PieceType::King, by))
            return true;
    }

    const auto slider = [&](const Step& step, PieceType other) {
        int f = file + step.file, r = rank + step.rank;
        while (onBoard(f, r)) {
            const Piece piece = at(makeSquare(f, r));
            if (!piece.empty()) {
                return piece.color == by && (piece.type == other || piece.type == PieceType::Queen);
            }
            f += step.file;
            r += step.rank;
        }
        return false;
    };
    for (const Step& step : kDiagonals) {
        if (slider(step, PieceType::Bishop))
            return true;
    }
    for (const Step& step : kStraights) {
        if (slider(step, PieceType::Rook))
            return true;
    }
    return false;
}

bool Position::inCheck() const {
    return isAttacked(kingSquare(_turn), opposite(_turn));
}

// --- move generation -----------------------------------------------------------

void Position::generatePseudoLegal(std::vector<Move>& out, Square onlyFrom) const {
    const Color us = _turn, them = opposite(_turn);

    const auto add = [&](Square from, Square to, PieceType piece, PieceType promotion = PieceType::None) {
        Move move;
        move.from      = from;
        move.to        = to;
        move.promotion = promotion;
        move.piece     = piece;
        move.captured  = at(to).type;
        out.push_back(move);
    };

    const Square first = onlyFrom == kNoSquare ? 0 : onlyFrom;
    const Square last  = onlyFrom == kNoSquare ? 63 : onlyFrom;

    for (Square from = first; from <= last; ++from) {
        const Piece piece = at(from);
        if (piece.empty() || piece.color != us)
            continue;
        const int file = fileOf(from), rank = rankOf(from);

        switch (piece.type) {
        case PieceType::Pawn: {
            const int forward   = us == Color::White ? 1 : -1;
            const int homeRank  = us == Color::White ? 1 : 6;
            const int lastRank  = us == Color::White ? 7 : 0;
            const auto addPawn = [&](Square to) {
                if (rankOf(to) == lastRank) {
                    for (PieceType promotion :
                         {PieceType::Queen, PieceType::Rook, PieceType::Bishop, PieceType::Knight})
                        add(from, to, PieceType::Pawn, promotion);
                } else {
                    add(from, to, PieceType::Pawn);
                }
            };

            if (onBoard(file, rank + forward) && at(makeSquare(file, rank + forward)).empty()) {
                addPawn(makeSquare(file, rank + forward));
                if (rank == homeRank && at(makeSquare(file, rank + 2 * forward)).empty())
                    add(from, makeSquare(file, rank + 2 * forward), PieceType::Pawn);
            }
            for (int df : {-1, 1}) {
                if (!onBoard(file + df, rank + forward))
                    continue;
                const Square to     = makeSquare(file + df, rank + forward);
                const Piece  target = at(to);
                if (!target.empty() && target.color == them) {
                    addPawn(to);
                } else if (to == _enPassant && target.empty()) {
                    Move move;
                    move.from      = from;
                    move.to        = to;
                    move.piece     = PieceType::Pawn;
                    move.captured  = PieceType::Pawn;
                    move.enPassant = true;
                    out.push_back(move);
                }
            }
            break;
        }

        case PieceType::Knight:
        case PieceType::King: {
            const auto& steps = piece.type == PieceType::Knight ? kKnightSteps : kKingSteps;
            for (const Step& step : steps) {
                if (!onBoard(file + step.file, rank + step.rank))
                    continue;
                const Square to     = makeSquare(file + step.file, rank + step.rank);
                const Piece  target = at(to);
                if (target.empty() || target.color == them)
                    add(from, to, piece.type);
            }

            if (piece.type == PieceType::King) {
                // Castling. The king must be at home with the right intact, the
                // squares between king and rook empty, and the king may not be
                // in check, pass through check or land in check. (Landing is
                // covered by the legality filter like any other move; the
                // square it passes over is checked here.)
                const Square home = us == Color::White ? kE1 : kE8;
                if (from != home || isAttacked(home, them))
                    break;
                if (canCastle(us, true) && at(home + 1).empty() && at(home + 2).empty() &&
                    !isAttacked(home + 1, them)) {
                    Move move;
                    move.from           = home;
                    move.to             = home + 2;
                    move.piece          = PieceType::King;
                    move.castleKingSide = true;
                    out.push_back(move);
                }
                if (canCastle(us, false) && at(home - 1).empty() && at(home - 2).empty() &&
                    at(home - 3).empty() && !isAttacked(home - 1, them)) {
                    Move move;
                    move.from            = home;
                    move.to              = home - 2;
                    move.piece           = PieceType::King;
                    move.castleQueenSide = true;
                    out.push_back(move);
                }
            }
            break;
        }

        case PieceType::Bishop:
        case PieceType::Rook:
        case PieceType::Queen: {
            const auto slide = [&](const Step& step) {
                int f = file + step.file, r = rank + step.rank;
                while (onBoard(f, r)) {
                    const Square to     = makeSquare(f, r);
                    const Piece  target = at(to);
                    if (target.empty()) {
                        add(from, to, piece.type);
                    } else {
                        if (target.color == them)
                            add(from, to, piece.type);
                        break;
                    }
                    f += step.file;
                    r += step.rank;
                }
            };
            if (piece.type != PieceType::Rook) {
                for (const Step& step : kDiagonals)
                    slide(step);
            }
            if (piece.type != PieceType::Bishop) {
                for (const Step& step : kStraights)
                    slide(step);
            }
            break;
        }

        case PieceType::None:
            break;
        }
    }
}

bool Position::leavesKingSafe(const Move& move) const {
    Position next = *this;
    next.apply(move);
    // After apply() the turn has passed, so "them" is now the side to move.
    return !next.isAttacked(next.kingSquare(_turn), next._turn);
}

std::vector<Move> Position::legalMoves() const {
    return legalMovesFrom(kNoSquare);
}

std::vector<Move> Position::legalMovesFrom(Square from) const {
    std::vector<Move> moves;
    if (from != kNoSquare && (from < 0 || from > 63))
        return moves;
    moves.reserve(from == kNoSquare ? 48 : 28);
    generatePseudoLegal(moves, from);
    moves.erase(std::remove_if(moves.begin(), moves.end(),
                               [this](const Move& move) { return !leavesKingSafe(move); }),
                moves.end());
    return moves;
}

bool Position::hasLegalMoves() const {
    std::vector<Move> moves;
    moves.reserve(48);
    generatePseudoLegal(moves, kNoSquare);
    return std::any_of(moves.begin(), moves.end(),
                       [this](const Move& move) { return leavesKingSafe(move); });
}

std::optional<Move> Position::findMove(Square from, Square to, PieceType promotion) const {
    for (const Move& move : legalMovesFrom(from)) {
        if (move.to == to && move.promotion == promotion)
            return move;
    }
    return std::nullopt;
}

bool Position::isPromotion(Square from, Square to) const {
    for (const Move& move : legalMovesFrom(from)) {
        if (move.to == to && move.promotion != PieceType::None)
            return true;
    }
    return false;
}

std::optional<Move> Position::parseUci(std::string_view text) const {
    if (text.size() != 4 && text.size() != 5)
        return std::nullopt;
    const Square from = parseSquare(text.substr(0, 2));
    const Square to   = parseSquare(text.substr(2, 2));
    if (from == kNoSquare || to == kNoSquare)
        return std::nullopt;

    PieceType promotion = PieceType::None;
    if (text.size() == 5) {
        promotion = typeFromLetter(text[4]);
        if (promotion == PieceType::None || promotion == PieceType::Pawn || promotion == PieceType::King)
            return std::nullopt;
    }
    return findMove(from, to, promotion);
}

// --- playing a move ------------------------------------------------------------

void Position::apply(const Move& move) {
    const Piece moving = at(move.from);
    const Color us     = moving.color;

    const bool isPawn    = moving.type == PieceType::Pawn;
    const bool enPassant = isPawn && move.to == _enPassant && fileOf(move.from) != fileOf(move.to) &&
                           at(move.to).empty();
    const bool capture   = enPassant || !at(move.to).empty();

    // Rights are lost by the square, not by the piece: anything leaving or
    // landing on a king's or rook's home square ends the right tied to it.
    // That covers the king moving, a rook moving and a rook being captured.
    const auto touch = [this](Square square) {
        switch (square) {
        case kE1: _castling &= static_cast<std::uint8_t>(~(kWhiteKing | kWhiteQueen)); break;
        case kH1: _castling &= static_cast<std::uint8_t>(~kWhiteKing); break;
        case kA1: _castling &= static_cast<std::uint8_t>(~kWhiteQueen); break;
        case kE8: _castling &= static_cast<std::uint8_t>(~(kBlackKing | kBlackQueen)); break;
        case kH8: _castling &= static_cast<std::uint8_t>(~kBlackKing); break;
        case kA8: _castling &= static_cast<std::uint8_t>(~kBlackQueen); break;
        default: break;
        }
    };
    touch(move.from);
    touch(move.to);

    if (enPassant) {
        // The captured pawn is beside the capturing one, not on the target.
        _board[static_cast<std::size_t>(makeSquare(fileOf(move.to), rankOf(move.from)))] = Piece{};
    }

    _board[static_cast<std::size_t>(move.to)]   = moving;
    _board[static_cast<std::size_t>(move.from)] = Piece{};

    if (isPawn && move.promotion != PieceType::None)
        _board[static_cast<std::size_t>(move.to)].type = move.promotion;

    // A king that moves two files is castling; bring the rook across.
    if (moving.type == PieceType::King && std::abs(fileOf(move.to) - fileOf(move.from)) == 2) {
        const bool kingSide = fileOf(move.to) > fileOf(move.from);
        const int  rank     = rankOf(move.from);
        const Square rookFrom = makeSquare(kingSide ? 7 : 0, rank);
        const Square rookTo   = makeSquare(kingSide ? 5 : 3, rank);
        _board[static_cast<std::size_t>(rookTo)]   = _board[static_cast<std::size_t>(rookFrom)];
        _board[static_cast<std::size_t>(rookFrom)] = Piece{};
    }

    _enPassant = kNoSquare;
    if (isPawn && std::abs(rankOf(move.to) - rankOf(move.from)) == 2)
        _enPassant = makeSquare(fileOf(move.from), (rankOf(move.from) + rankOf(move.to)) / 2);

    _halfmove = (isPawn || capture) ? 0 : _halfmove + 1;
    if (us == Color::Black)
        ++_fullmove;
    _turn = opposite(us);
}

Position Position::after(const Move& move) const {
    Position next = *this;
    next.apply(move);
    return next;
}

// --- notation ------------------------------------------------------------------

std::string Position::san(const Move& move) const {
    std::string text;

    if (move.castleKingSide) {
        text = "O-O";
    } else if (move.castleQueenSide) {
        text = "O-O-O";
    } else {
        if (move.piece == PieceType::Pawn) {
            if (move.isCapture())
                text += static_cast<char>('a' + fileOf(move.from));
        } else {
            text += pieceLetter(move.piece);

            // Two pieces of a kind that can reach the same square need telling
            // apart: by file if that is enough, else by rank, else by both.
            bool ambiguous = false, sameFile = false, sameRank = false;
            for (const Move& other : legalMoves()) {
                if (other.piece != move.piece || other.to != move.to || other.from == move.from)
                    continue;
                ambiguous = true;
                sameFile |= fileOf(other.from) == fileOf(move.from);
                sameRank |= rankOf(other.from) == rankOf(move.from);
            }
            if (ambiguous) {
                if (!sameFile) {
                    text += static_cast<char>('a' + fileOf(move.from));
                } else if (!sameRank) {
                    text += static_cast<char>('1' + rankOf(move.from));
                } else {
                    text += squareName(move.from);
                }
            }
        }

        if (move.isCapture())
            text += 'x';
        text += squareName(move.to);

        if (move.promotion != PieceType::None) {
            text += '=';
            text += pieceLetter(move.promotion);
        }
    }

    const Position next = after(move);
    if (next.inCheck())
        text += next.hasLegalMoves() ? '+' : '#';
    return text;
}

std::optional<Move> Position::parseSan(std::string_view text) const {
    // Rather than a second parser with its own idea of the rules, compare
    // against what san() would write for each legal move, ignoring the
    // decorations people add or leave out.
    const auto bare = [](std::string_view in) {
        std::string out;
        for (char c : in) {
            if (c == '+' || c == '#' || c == '!' || c == '?')
                continue;
            out += c == '0' ? 'O' : c;  // 0-0 is how castling is often typed
        }
        return out;
    };

    const std::string wanted = bare(text);
    if (wanted.empty())
        return std::nullopt;
    for (const Move& move : legalMoves()) {
        if (bare(san(move)) == wanted)
            return move;
    }
    return std::nullopt;
}

// --- draws by material ---------------------------------------------------------

bool Position::insufficientMaterial() const {
    int knights = 0, bishops = 0;
    bool lightBishop = false, darkBishop = false;

    for (Square square = 0; square < 64; ++square) {
        const Piece piece = at(square);
        switch (piece.type) {
        case PieceType::None:
        case PieceType::King:
            break;
        case PieceType::Knight:
            ++knights;
            break;
        case PieceType::Bishop:
            ++bishops;
            ((fileOf(square) + rankOf(square)) % 2 == 0 ? darkBishop : lightBishop) = true;
            break;
        default:
            // A pawn, rook or queen can always be turned into a mate.
            return false;
        }
    }

    if (knights == 0 && bishops == 0)
        return true;  // king against king
    if (knights == 1 && bishops == 0)
        return true;  // a lone knight
    if (knights == 0 && !(lightBishop && darkBishop))
        return true;  // any number of bishops, all on one colour
    return false;
}

}  // namespace chess
