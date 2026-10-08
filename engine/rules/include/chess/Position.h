// One position: where the pieces are, whose move it is, and the three things a
// diagram does not show — castling rights, the en passant square and the clocks.
//
// A plain 8x8 array rather than bitboards. This library answers questions for a
// person at a board — is this move legal, is this mate, how is it written — a
// few dozen times a second at most, and the simplest representation that is
// obviously right is the one worth having. The thinking is Stockfish's job.
#pragma once

#include "chess/Types.h"

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace chess {

inline constexpr std::string_view kStartFen =
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

class Position {
public:
    // An empty board, white to move. Mostly useful as a blank for tests.
    Position() = default;

    static Position initial();

    // Reads a FEN. The last two fields (the clocks) may be omitted. Castling
    // rights that the placement contradicts — no king or no rook where one
    // must be — are dropped rather than rejected.
    static std::optional<Position> fromFen(std::string_view fen, std::string* error = nullptr);
    std::string fen() const;

    Piece  at(Square square) const { return _board[static_cast<std::size_t>(square)]; }
    Color  sideToMove() const { return _turn; }
    Square enPassantSquare() const { return _enPassant; }
    bool   canCastle(Color color, bool kingSide) const;
    int    halfmoveClock() const { return _halfmove; }
    int    fullmoveNumber() const { return _fullmove; }

    Square kingSquare(Color color) const;
    bool   isAttacked(Square square, Color by) const;
    // Is the side to move in check?
    bool   inCheck() const;

    // Every legal move for the side to move, with its description filled in.
    std::vector<Move> legalMoves() const;
    std::vector<Move> legalMovesFrom(Square from) const;
    bool hasLegalMoves() const;

    // The legal move with this origin, destination and promotion, if there is
    // one. A pawn reaching the last rank needs the promotion piece named.
    std::optional<Move> findMove(Square from, Square to,
                                 PieceType promotion = PieceType::None) const;
    // Would moving from `from` to `to` be a promotion (so the piece must be
    // asked for)?
    bool isPromotion(Square from, Square to) const;

    std::optional<Move> parseUci(std::string_view text) const;

    // Standard algebraic notation, with the check or mate sign: "Nbd7", "exd6",
    // "O-O", "e8=Q+", "Qh7#". The move must be legal here.
    std::string san(const Move& move) const;
    std::optional<Move> parseSan(std::string_view text) const;

    // Plays a move. Only origin, destination and promotion are read; whether it
    // is a capture, a castle or en passant is worked out from the board, so a
    // move parsed from text is as good as one from the generator. The move is
    // assumed to be legal.
    void apply(const Move& move);
    Position after(const Move& move) const;

    // Neither side could mate even with the other's help: bare kings, a lone
    // minor piece, or only bishops that all stand on one colour.
    bool insufficientMaterial() const;

    // What "the same position" means for the repetition rule: placement, side
    // to move, castling rights, and the en passant square only when a capture
    // there is really possible.
    std::string repetitionKey() const;

    bool operator==(const Position&) const = default;

private:
    void generatePseudoLegal(std::vector<Move>& out, Square onlyFrom) const;
    bool leavesKingSafe(const Move& move) const;
    void placementTo(std::string& out) const;

    std::array<Piece, 64> _board{};
    Color        _turn      = Color::White;
    std::uint8_t _castling  = 0;  // bit 0 K, 1 Q, 2 k, 3 q
    Square       _enPassant = kNoSquare;
    int          _halfmove  = 0;
    int          _fullmove  = 1;
};

}  // namespace chess
