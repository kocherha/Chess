#include "chess/Position.h"

#include "Board.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <utility>

namespace chess {

namespace {

using namespace board;

constexpr Square kA1 = 0, kE1 = 4, kH1 = 7, kA8 = 56, kE8 = 60, kH8 = 63;

// Says what is wrong with a FEN, to whoever asked to know: `return fail(...)`.
std::nullopt_t fail(std::string* error, std::string message) {
    if (error != nullptr)
        *error = std::move(message);
    return std::nullopt;
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

// --- FEN -----------------------------------------------------------------------

Position Position::initial() {
    static const Position start = *fromFen(kStartFen);  // read once
    return start;
}

std::optional<Position> Position::fromFen(std::string_view fen, std::string* error) {
    const auto fields = splitFields(fen);
    if (fields.size() < 4)
        return fail(error, "a FEN needs at least four fields");

    Position position;
    if (const char* wrong = position.readBoard(fields[0]))
        return fail(error, wrong);

    if (fields[1] == "w")
        position._turn = Color::White;
    else if (fields[1] == "b")
        position._turn = Color::Black;
    else
        return fail(error, "the side to move must be w or b");

    if (!position.readCastling(fields[2]))
        return fail(error, "malformed castling rights");
    if (!position.readEnPassant(fields[3]))
        return fail(error, "malformed en passant square");

    if (fields.size() > 4 && !parseNumber(fields[4], position._halfmove))
        return fail(error, "malformed halfmove clock");
    if (fields.size() > 5 && (!parseNumber(fields[5], position._fullmove) || position._fullmove < 1))
        return fail(error, "malformed move number");

    // The side that has just moved cannot have left its own king attacked.
    if (position.isAttacked(position.kingSquare(opposite(position._turn)), position._turn))
        return fail(error, "the side not to move is in check");

    return position;
}

// The first field of a FEN: ranks from the eighth down to the first, files a
// to h. What is wrong with it, or nullptr.
const char* Position::readBoard(std::string_view field) {
    const auto put = [this](Square square, Piece piece) { _board[static_cast<std::size_t>(square)] = piece; };
    if (!readPlacement(field, put))
        return "malformed piece placement";
    for (int file = 0; file < 8; ++file)
        if (at(makeSquare(file, 0)).type == PieceType::Pawn || at(makeSquare(file, 7)).type == PieceType::Pawn)
            return "a pawn cannot stand on the first or last rank";
    for (const Color color : {Color::White, Color::Black})
        if (std::count(_board.begin(), _board.end(), Piece{PieceType::King, color}) != 1)
            return "each side needs exactly one king";
    return nullptr;
}

// The third field: "KQkq", any of them, or "-". False if it is anything else.
bool Position::readCastling(std::string_view field) {
    if (field != "-") {
        for (char c : field) {
            switch (c) {
            case 'K': _castling |= kWhiteKing; break;
            case 'Q': _castling |= kWhiteQueen; break;
            case 'k': _castling |= kBlackKing; break;
            case 'q': _castling |= kBlackQueen; break;
            default:  return false;
            }
        }
    }
    // A right is only worth keeping if the king and that rook are at home.
    const auto keepIf = [this](std::uint8_t bit, Square king, Square rook, Color color) {
        if (!at(king).is(PieceType::King, color) || !at(rook).is(PieceType::Rook, color))
            _castling &= static_cast<std::uint8_t>(~bit);
    };
    keepIf(kWhiteKing, kE1, kH1, Color::White);
    keepIf(kWhiteQueen, kE1, kA1, Color::White);
    keepIf(kBlackKing, kE8, kH8, Color::Black);
    keepIf(kBlackQueen, kE8, kA8, Color::Black);
    return true;
}

// The fourth field: the square behind a pawn that has just advanced two —
// rank 6 with white to move, rank 3 with black — or "-". Kept only if such a
// pawn stands in front of it. False if it is no such square. The side to
// move has to be known already.
bool Position::readEnPassant(std::string_view field) {
    if (field == "-")
        return true;
    const Square square = parseSquare(field);
    if (square == kNoSquare || rankOf(square) != (_turn == Color::White ? 5 : 2))
        return false;
    const Square pawn = makeSquare(fileOf(square), _turn == Color::White ? 4 : 3);
    if (at(pawn).is(PieceType::Pawn, opposite(_turn)) && at(square).empty())
        _enPassant = square;
    return true;
}

void Position::placementTo(std::string& out) const {
    board::writePlacement(out, [this](Square square) { return at(square); });
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

bool Position::canCastle(Color color, Wing wing) const {
    return (_castling & castleBit(color, wing)) != 0;
}

Square Position::kingSquare(Color color) const {
    return findKing([this](Square square) { return at(square); }, color);
}

bool Position::isAttacked(Square square, Color by) const {
    if (square == kNoSquare)
        return false;
    const auto piece = [this](Square on) { return at(on); };
    return board::findAttackers(piece, square, by, [](Square) { return true; });  // one is enough
}

bool Position::inCheck() const {
    return isAttacked(kingSquare(_turn), opposite(_turn));
}

// --- move generation -----------------------------------------------------------

namespace {

// An ordinary move: from, to, what moved, what it takes, what a pawn becomes.
void addMove(std::vector<Move>& out, const Position& position, Square from, Square to, PieceType piece,
             PieceType promotion = PieceType::None) {
    Move move;
    move.from      = from;
    move.to        = to;
    move.promotion = promotion;
    move.piece     = piece;
    move.captured  = position.at(to).type;
    out.push_back(move);
}

}  // namespace

void Position::generatePseudoLegal(std::vector<Move>& out, Square onlyFrom) const {
    const Square first = onlyFrom == kNoSquare ? 0 : onlyFrom;
    const Square last  = onlyFrom == kNoSquare ? 63 : onlyFrom;

    for (Square from = first; from <= last; ++from) {
        const Piece piece = at(from);
        if (piece.empty() || piece.color != _turn)
            continue;
        switch (piece.type) {
        case PieceType::Pawn:
            pawnMoves(from, out);
            break;
        case PieceType::Knight:
            stepMoves(from, piece.type, out);
            break;
        case PieceType::King:
            stepMoves(from, piece.type, out);
            castlingMoves(from, out);
            break;
        case PieceType::Bishop:
        case PieceType::Rook:
        case PieceType::Queen:
            slideMoves(from, piece.type, out);
            break;
        case PieceType::None:
            break;
        }
    }
}

void Position::pawnMoves(Square from, std::vector<Move>& out) const {
    const Color us = _turn, them = opposite(_turn);
    const int file = fileOf(from), rank = rankOf(from);
    const int forward  = us == Color::White ? 1 : -1;
    const int homeRank = us == Color::White ? 1 : 6;
    const int lastRank = us == Color::White ? 7 : 0;

    // A pawn that reaches the last rank becomes any of four pieces: four moves.
    const auto addPawn = [&](Square to) {
        if (rankOf(to) == lastRank) {
            for (PieceType promotion : {PieceType::Queen, PieceType::Rook, PieceType::Bishop, PieceType::Knight})
                addMove(out, *this, from, to, PieceType::Pawn, promotion);
        } else {
            addMove(out, *this, from, to, PieceType::Pawn);
        }
    };

    if (onBoard(file, rank + forward) && at(makeSquare(file, rank + forward)).empty()) {
        addPawn(makeSquare(file, rank + forward));
        if (rank == homeRank && at(makeSquare(file, rank + 2 * forward)).empty())
            addMove(out, *this, from, makeSquare(file, rank + 2 * forward), PieceType::Pawn);
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
}

void Position::stepMoves(Square from, PieceType piece, std::vector<Move>& out) const {
    const Color them = opposite(_turn);
    const int file = fileOf(from), rank = rankOf(from);
    const auto& steps = piece == PieceType::Knight ? kKnightSteps : kKingSteps;
    for (const Step& step : steps) {
        if (!onBoard(file + step.file, rank + step.rank))
            continue;
        const Square to     = makeSquare(file + step.file, rank + step.rank);
        const Piece  target = at(to);
        if (target.empty() || target.color == them)
            addMove(out, *this, from, to, piece);
    }
}

void Position::slideMoves(Square from, PieceType piece, std::vector<Move>& out) const {
    const Color them = opposite(_turn);
    const int file = fileOf(from), rank = rankOf(from);
    // Along one line until the edge, a piece of its own, or one it takes.
    const auto slide = [&](const Step& step) {
        int f = file + step.file, r = rank + step.rank;
        while (onBoard(f, r)) {
            const Square to     = makeSquare(f, r);
            const Piece  target = at(to);
            if (target.empty()) {
                addMove(out, *this, from, to, piece);
            } else {
                if (target.color == them)
                    addMove(out, *this, from, to, piece);
                break;
            }
            f += step.file;
            r += step.rank;
        }
    };
    if (piece != PieceType::Rook) {
        for (const Step& step : kDiagonals)
            slide(step);
    }
    if (piece != PieceType::Bishop) {
        for (const Step& step : kStraights)
            slide(step);
    }
}

// The king must be at home with the right intact, the squares between king
// and rook empty, and the king may not be in check, pass through check or
// land in check. (Landing is covered by the legality filter like any other
// move; the square it passes over is checked here.)
void Position::castlingMoves(Square from, std::vector<Move>& out) const {
    const Color  us = _turn, them = opposite(_turn);
    const Square home = us == Color::White ? kE1 : kE8;
    if (from != home || isAttacked(home, them))
        return;
    if (canCastle(us, Wing::King) && at(home + 1).empty() && at(home + 2).empty() && !isAttacked(home + 1, them)) {
        Move move;
        move.from           = home;
        move.to             = home + 2;
        move.piece          = PieceType::King;
        move.castleKingSide = true;
        out.push_back(move);
    }
    if (canCastle(us, Wing::Queen) && at(home - 1).empty() && at(home - 2).empty() && at(home - 3).empty() &&
        !isAttacked(home - 1, them)) {
        Move move;
        move.from            = home;
        move.to              = home - 2;
        move.piece           = PieceType::King;
        move.castleQueenSide = true;
        out.push_back(move);
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

std::optional<Position> Position::passed() const {
    if (inCheck())
        return std::nullopt;
    Position other   = *this;
    other._turn      = opposite(_turn);
    other._enPassant = kNoSquare;  // that chance was this side's, and it is not taking it
    return other;
}

int Position::exchange(const Move& capture) const {
    const auto move = findMove(capture.from, capture.to, capture.promotion);
    if (!move || !move->isCapture())
        return 0;

    // What comes off the board now; a pawn that promotes while taking is
    // worth what it becomes from here on.
    int gain = pieceValue(move->captured);
    if (move->promotion != PieceType::None)
        gain += pieceValue(move->promotion) - pieceValue(PieceType::Pawn);

    // The other side takes back with its cheapest piece — if that is worth
    // doing. Legal moves only, so a pinned defender does not count and a king
    // does not take a protected piece.
    const Position next = after(*move);
    const Move* cheapest = nullptr;
    std::vector<Move> replies = next.legalMoves();
    for (const Move& reply : replies) {
        if (reply.to != move->to || !reply.isCapture())
            continue;
        if (reply.promotion != PieceType::None && reply.promotion != PieceType::Queen)
            continue;  // one promotion is enough to consider
        // A king is the last piece to take with: it is worth nothing here
        // only because it cannot be lost.
        const auto cost = [](const Move& m) { return m.piece == PieceType::King ? 100 : pieceValue(m.piece); };
        if (cheapest == nullptr || cost(reply) < cost(*cheapest))
            cheapest = &reply;
    }
    if (cheapest == nullptr)
        return gain;
    return gain - std::max(0, next.exchange(*cheapest));
}

// --- notation ------------------------------------------------------------------

std::string Position::san(const Move& move) const {
    // Bare - from a caller that only knows where it goes: as the generator
    // would describe it, so that a capture gets its "x" and a castle its O-O.
    if (move.piece == PieceType::None) {
        if (const auto described = findMove(move.from, move.to, move.promotion))
            return san(*described);
    }

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
            (lightSquare(square) ? lightBishop : darkBishop) = true;
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
