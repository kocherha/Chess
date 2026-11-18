#include "chess/Types.h"

#include <cctype>

namespace chess {

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

}  // namespace chess
