// The vocabulary: colours, pieces, squares and moves.
#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace chess {

enum class Color : std::uint8_t { White, Black };

constexpr Color opposite(Color color) {
    return color == Color::White ? Color::Black : Color::White;
}

enum class PieceType : std::uint8_t { None, Pawn, Knight, Bishop, Rook, Queen, King };

struct Piece {
    PieceType type  = PieceType::None;
    Color     color = Color::White;

    constexpr bool empty() const { return type == PieceType::None; }
    constexpr bool is(PieceType t, Color c) const { return type == t && color == c; }
    constexpr bool operator==(const Piece&) const = default;
};

// A square is a number from 0 to 63: a1 = 0, b1 = 1, ... h1 = 7, a2 = 8, ...
// h8 = 63. So the file is the low three bits and the rank is the rest.
using Square = int;
constexpr Square kNoSquare = -1;

constexpr int fileOf(Square square) { return square & 7; }
constexpr int rankOf(Square square) { return square >> 3; }
constexpr Square makeSquare(int file, int rank) { return rank * 8 + file; }
constexpr bool onBoard(int file, int rank) { return file >= 0 && file < 8 && rank >= 0 && rank < 8; }

// "e4" and back. parseSquare returns kNoSquare for anything that is not a
// square name.
std::string squareName(Square square);
Square parseSquare(std::string_view name);

// The English letter used in notation: P N B R Q K ('?' for None).
char pieceLetter(PieceType type);
// The FEN character: upper case for white, lower case for black.
char pieceChar(Piece piece);

// Conventional material values in pawns (king = 0). For showing who is ahead,
// not for playing.
int pieceValue(PieceType type);

// A move is fully identified by where it starts, where it ends and what a pawn
// becomes. Everything else is a description the generator fills in so that
// callers do not have to work it out again from the board.
struct Move {
    Square    from      = kNoSquare;
    Square    to        = kNoSquare;
    PieceType promotion = PieceType::None;

    PieceType piece    = PieceType::None;  // what moved
    PieceType captured = PieceType::None;  // what it took, if anything
    bool enPassant       = false;
    bool castleKingSide  = false;
    bool castleQueenSide = false;

    bool isCapture() const { return captured != PieceType::None; }
    bool isCastle() const { return castleKingSide || castleQueenSide; }

    // Identity only: two moves are the same move if they would be written the
    // same way in UCI.
    bool sameAs(const Move& other) const {
        return from == other.from && to == other.to && promotion == other.promotion;
    }

    // Long algebraic, as UCI engines speak it: "e2e4", "e7e8q", "e1g1".
    std::string uci() const;
};

}  // namespace chess
