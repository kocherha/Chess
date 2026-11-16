// A position being set up by hand: any arrangement of pieces at all, and an
// account of why it could or could not have come about in a game.
//
// A Position is always a position a game could be in — Position::fromFen
// refuses anything else — so it cannot be the thing a person edits: half way
// through setting up a problem the board has one king, or three pawns on the
// eighth rank, and that is fine, it is not finished. A Setup holds whatever is
// on the board and says, every time it is asked, what stands between it and a
// legal position. Once nothing does, position() gives the Position.
//
// "Could have come about" is decided the way problem composers and arbiters
// decide it, as far as can be done without a full retrograde search: the
// count of pieces, where the pawns stand and how many captures that took, the
// kings, and whether the check on the board could have been given by the
// move that was just played. A position that passes all of it may still be
// unreachable in some subtle way; one that fails any of it certainly is.
#pragma once

#include "chess/Position.h"
#include "chess/Types.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace chess {

// What is wrong with a set-up position.
enum class Flaw : std::uint8_t {
    // Not impossible, just not finished: a side has no king yet. While it
    // has none, nothing that is about kings is checked for that side.
    MissingKing,

    ExtraKing,            // more than one king of a colour
    PawnOnEdge,           // a pawn on the first or the last rank
    KingsTouching,        // the two kings stand next to each other
    WaitingSideInCheck,   // the side that has just moved is in check
    TooManyCheckers,      // the side to move is in check from three or more pieces
    ImpossibleCheck,      // a check that no single move could have given
    TooManyPawns,         // more than eight pawns of a colour
    TooManyPieces,        // more promoted pieces than there were pawns to promote
    PawnCaptures,         // the pawns stand where only more captures than were possible could put them
    BadEnPassant,         // the en passant square does not fit the check on the board
};

struct Issue {
    Flaw  flaw  = Flaw::MissingKing;
    Color color = Color::White;    // whose pieces it is about
    std::vector<Square> squares;   // what to point at on the board
    // For the counting flaws: how many were needed, and how many there could
    // have been. TooManyPieces: promotions needed, pawns missing.
    // PawnCaptures: captures the pawns needed, opposing pieces missing (or
    // need = -1 when no number of captures would do).
    int need = 0, have = 0;

    bool operator==(const Issue&) const = default;
};

class Setup {
public:
    // An empty board, white to move.
    Setup() = default;

    static Setup initial();
    static Setup from(const Position& position);

    // Reads a FEN without asking whether the position is possible: only the
    // placement has to be well formed (eight ranks of eight squares). The
    // other fields may be missing or partly wrong; what cannot be read is left
    // at its default. Empty if the placement cannot be read.
    static std::optional<Setup> fromFen(std::string_view fen);

    // The FEN of what is on the board, possible or not. Castling rights and
    // the en passant square are written only when the placement allows them.
    std::string fen() const;

    Piece at(Square square) const { return _board[static_cast<std::size_t>(square)]; }
    void  put(Square square, Piece piece);
    void  remove(Square square) { put(square, Piece{}); }
    void  clear();

    Color sideToMove() const { return _turn; }
    void  setSideToMove(Color color);

    // Castling. A right is wanted by the person setting up the position, and
    // it is in force only when the king and that rook are on their squares;
    // a right that is wanted but not possible comes back by itself when they
    // are put back.
    bool castlingPossible(Color color, bool kingSide) const;
    bool canCastle(Color color, bool kingSide) const;  // wanted and possible
    bool castlingWanted(Color color, bool kingSide) const;
    void setCastling(Color color, bool kingSide, bool wanted);

    // En passant: the squares behind an opposing pawn that may just have
    // advanced two, where a pawn of the side to move could take it. Set to one
    // of them or to kNoSquare. Like castling, the square is kept while it no
    // longer fits and is in force only while it does.
    std::vector<Square> enPassantCandidates() const;
    Square enPassant() const;  // in force, or kNoSquare
    void   setEnPassant(Square square) { _enPassant = square; }

    int  fullmoveNumber() const { return _fullmove; }
    void setFullmoveNumber(int number) { _fullmove = number < 1 ? 1 : number; }

    // Everything that stands between this and a legal position, roughly in
    // the order a person would want to put them right: missing kings first.
    std::vector<Issue> issues() const;

    // The position, once there is nothing in issues().
    std::optional<Position> position() const;

    int count(Piece piece) const;
    Square kingSquare(Color color) const;  // the first one, or kNoSquare

    bool operator==(const Setup&) const = default;

private:
    // The pieces of `by` that attack `square`.
    std::vector<Square> attackers(Square square, Color by) const;

    std::array<Piece, 64> _board{};
    Color        _turn      = Color::White;
    std::uint8_t _castling  = 0;  // wanted: bit 0 K, 1 Q, 2 k, 3 q
    Square       _enPassant = kNoSquare;  // wanted
    int          _halfmove  = 0;
    int          _fullmove  = 1;
};

}  // namespace chess
