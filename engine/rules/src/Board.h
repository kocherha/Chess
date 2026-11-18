// What Position.cpp and Setup.cpp both need: the ways the pieces step across
// a board, how castling rights are kept as bits, and the two small pieces of
// reading a FEN. Private to the rules library — it is not under include/.
#pragma once

#include "chess/Types.h"

#include <cctype>
#include <cstdint>
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

}  // namespace chess::board
