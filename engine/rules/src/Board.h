// What Position.cpp and Setup.cpp both need: the ways the pieces step across
// a board, which pieces attack a square, how castling rights are kept as bits,
// and the small pieces of reading and writing a FEN. Private to the rules
// library — it is not under include/.
//
// Both kinds of board are read through `at`: anything that gives the piece on
// a square.
#pragma once

#include "chess/Types.h"

#include <cctype>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace chess::board {

struct Step {
    int file, rank;
};
inline constexpr Step kKnightSteps[] = {{1, 2}, {2, 1}, {2, -1}, {1, -2}, {-1, -2}, {-2, -1}, {-2, 1}, {-1, 2}};
inline constexpr Step kKingSteps[]   = {{1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}, {0, -1}, {1, -1}};
inline constexpr Step kDiagonals[]   = {{1, 1}, {-1, 1}, {-1, -1}, {1, -1}};
inline constexpr Step kStraights[]   = {{1, 0}, {0, 1}, {-1, 0}, {0, -1}};

// Where `color`'s king stands, or kNoSquare.
template <class At>
Square findKing(const At& at, Color color) {
    for (Square square = 0; square < 64; ++square)
        if (at(square).is(PieceType::King, color))
            return square;
    return kNoSquare;
}

// Every piece of `by` that attacks `target`, handed to `found` one at a time —
// pawns, knights, the king, then along the lines — until `found` returns
// true. True if it did.
template <class At, class Found>
bool findAttackers(const At& at, Square target, Color by, Found&& found) {
    const int file = fileOf(target), rank = rankOf(target);
    const auto is = [&](int f, int r, PieceType type) {
        return onBoard(f, r) && at(makeSquare(f, r)).is(type, by) && found(makeSquare(f, r));
    };

    // A pawn attacks diagonally forward, so the attacker stands one rank
    // *behind* the square from its own point of view.
    const int pawnRank = by == Color::White ? rank - 1 : rank + 1;
    for (int df : {-1, 1})
        if (is(file + df, pawnRank, PieceType::Pawn))
            return true;
    for (const Step& step : kKnightSteps)
        if (is(file + step.file, rank + step.rank, PieceType::Knight))
            return true;
    for (const Step& step : kKingSteps)
        if (is(file + step.file, rank + step.rank, PieceType::King))
            return true;

    // Along a line, the first piece met is the only one that can attack.
    const auto line = [&](const Step& step, PieceType kind) {
        for (int f = file + step.file, r = rank + step.rank; onBoard(f, r); f += step.file, r += step.rank) {
            const Piece piece = at(makeSquare(f, r));
            if (piece.empty())
                continue;
            return piece.color == by && (piece.type == kind || piece.type == PieceType::Queen) &&
                   found(makeSquare(f, r));
        }
        return false;
    };
    for (const Step& step : kDiagonals)
        if (line(step, PieceType::Bishop))
            return true;
    for (const Step& step : kStraights)
        if (line(step, PieceType::Rook))
            return true;
    return false;
}

// Castling rights, one bit each, in the order a FEN writes them: K Q k q.
inline constexpr std::uint8_t kWhiteKing = 1, kWhiteQueen = 2, kBlackKing = 4, kBlackQueen = 8;

constexpr std::uint8_t castleBit(Color color, bool kingSide) {
    return color == Color::White ? (kingSide ? kWhiteKing : kWhiteQueen)
                                 : (kingSide ? kBlackKing : kBlackQueen);
}

// The piece a FEN letter stands for, either case; None for any other letter.
inline PieceType typeFromLetter(char letter) {
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

// The fields of a FEN: the text between runs of white space.
inline std::vector<std::string_view> splitFields(std::string_view text) {
    std::vector<std::string_view> fields;
    std::size_t i = 0;
    while (i < text.size()) {
        while (i < text.size() && std::isspace(static_cast<unsigned char>(text[i])))
            ++i;
        const std::size_t start = i;
        while (i < text.size() && !std::isspace(static_cast<unsigned char>(text[i])))
            ++i;
        if (i > start)
            fields.push_back(text.substr(start, i - start));
    }
    return fields;
}

// The first field of a FEN read back: each piece it names is handed to
// `put(square, piece)`. False unless it is eight ranks of eight squares, from
// rank 8 down, in piece letters and digits.
template <class Put>
bool readPlacement(std::string_view field, Put&& put) {
    int file = 0, rank = 7;
    for (char c : field) {
        if (c == '/') {
            if (file != 8 || rank == 0)
                return false;
            file = 0;
            --rank;
        } else if (c >= '1' && c <= '8') {
            file += c - '0';
            if (file > 8)
                return false;
        } else {
            const PieceType type = typeFromLetter(c);
            if (type == PieceType::None || file > 7)
                return false;
            const Color color = std::isupper(static_cast<unsigned char>(c)) ? Color::White : Color::Black;
            put(makeSquare(file, rank), Piece{type, color});
            ++file;
        }
    }
    return rank == 0 && file == 8;
}

// The first field of a FEN: the board from rank 8 down, runs of empty squares
// as a digit.
template <class At>
void writePlacement(std::string& out, const At& at) {
    for (int rank = 7; rank >= 0; --rank) {
        int gap = 0;
        for (int file = 0; file < 8; ++file) {
            const Piece piece = at(makeSquare(file, rank));
            if (piece.empty()) {
                ++gap;
                continue;
            }
            if (gap > 0)
                out += static_cast<char>('0' + gap);
            gap = 0;
            out += pieceChar(piece);
        }
        if (gap > 0)
            out += static_cast<char>('0' + gap);
        if (rank > 0)
            out += '/';
    }
}

}  // namespace chess::board
