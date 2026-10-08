#include "Look.h"

#include "Shapes.h"
#include "Strings.h"
#include "Theme.h"

#include <algorithm>
#include <cmath>
#include <string>

using namespace ax;

namespace look {

namespace {

using theme::rgb;

const Color4F kNone = rgb(0, 0, 0, 0.0f);

int gPieces = 0;
int gBoard  = 0;

// The keys the choice is remembered under.
constexpr const char* kPiecesKey = "pieces";
constexpr const char* kBoardKey  = "board";

template <typename Item>
int indexOf(const std::vector<Item>& items, std::string_view id) {
    for (std::size_t i = 0; i < items.size(); ++i) {
        if (id == items[i].id)
            return static_cast<int>(i);
    }
    return -1;
}

}  // namespace

const std::vector<PieceSet>& pieceSets() {
    static const std::vector<PieceSet> sets = {
        // Colin Burnett's set: the one most players have looked at for years.
        {"classic", text::kPiecesClassic, "pieces/classic"},
        // Drawn for this game; see art/gothic.py.
        {"gothic", text::kPiecesGothic, "pieces/gothic"},
    };
    return sets;
}

const std::vector<BoardStyle>& boards() {
    static const std::vector<BoardStyle> styles = {
        // The brown pair of a wooden club board.
        {"wood", text::kBoardWood,
         rgb(240, 217, 181), rgb(181, 136, 99), rgb(48, 38, 30), kNone, 1.0f, false,
         rgb(205, 210, 60, 0.42f), rgb(60, 130, 60, 0.55f), rgb(30, 80, 30, 0.38f), rgb(226, 48, 40, 0.85f)},

        // Worn stone and oxblood, laid as tiles with a brass stud where four
        // of them meet. The marks are candle colours: green would be lost on
        // the red, and red on the red would be no warning at all.
        {"gothic", text::kBoardGothic,
         rgb(203, 195, 180), rgb(112, 44, 50), rgb(22, 18, 22), rgb(168, 134, 76), 0.3f, true,
         rgb(226, 176, 60, 0.42f), rgb(236, 196, 84, 0.72f), rgb(18, 10, 12, 0.46f), rgb(255, 150, 40, 0.92f)},

        // Cool grey: the least colour of the four, for pieces to stand out on.
        {"marble", text::kBoardMarble,
         rgb(228, 230, 234), rgb(141, 150, 167), rgb(46, 50, 60), kNone, 1.0f, false,
         rgb(120, 170, 230, 0.45f), rgb(60, 130, 200, 0.55f), rgb(24, 40, 70, 0.36f), rgb(226, 48, 40, 0.85f)},

        // The green baize of a tournament hall.
        {"emerald", text::kBoardEmerald,
         rgb(235, 236, 208), rgb(115, 149, 82), rgb(38, 50, 32), kNone, 1.0f, false,
         rgb(245, 246, 105, 0.52f), rgb(250, 200, 60, 0.62f), rgb(20, 40, 20, 0.34f), rgb(226, 48, 40, 0.85f)},
    };
    return styles;
}

const PieceSet& pieces() {
    return pieceSets()[static_cast<std::size_t>(gPieces)];
}

const BoardStyle& board() {
    return boards()[static_cast<std::size_t>(gBoard)];
}

int pieceIndex() {
    return gPieces;
}

int boardIndex() {
    return gBoard;
}

void restore() {
    // By id rather than by number, so that the tables can be reordered or
    // added to without yesterday's choice turning into a different one.
    auto* store = UserDefault::getInstance();
    choosePieces(store->getStringForKey(kPiecesKey, ""), false);
    chooseBoard(store->getStringForKey(kBoardKey, ""), false);
}

void choosePieces(int index, bool keep) {
    if (index < 0 || index >= static_cast<int>(pieceSets().size()))
        return;
    gPieces = index;
    if (keep) {
        auto* store = UserDefault::getInstance();
        store->setStringForKey(kPiecesKey, pieces().id);
        store->flush();
    }
}

void chooseBoard(int index, bool keep) {
    if (index < 0 || index >= static_cast<int>(boards().size()))
        return;
    gBoard = index;
    if (keep) {
        auto* store = UserDefault::getInstance();
        store->setStringForKey(kBoardKey, board().id);
        store->flush();
    }
}

bool choosePieces(std::string_view id, bool keep) {
    const int index = indexOf(pieceSets(), id);
    choosePieces(index, keep);
    return index >= 0;
}

bool chooseBoard(std::string_view id, bool keep) {
    const int index = indexOf(boards(), id);
    chooseBoard(index, keep);
    return index >= 0;
}

// --- pieces ------------------------------------------------------------------------

Sprite* sprite(const PieceSet& set, chess::Piece piece, float square) {
    std::string name = set.folder;
    name += '/';
    name += piece.color == chess::Color::White ? 'w' : 'b';
    name += chess::pieceLetter(piece.type);
    name += ".png";

    auto* made = Sprite::create(name);
    if (made == nullptr)
        return nullptr;

    // The artwork is far larger than a square on screen, and scaled down with
    // plain bilinear filtering its thin outlines shimmer. Mipmaps fix that;
    // the texture is shared by every sprite of this piece, so this only does
    // work the first time.
    Texture2D* texture = made->getTexture();
    if (texture != nullptr && !texture->hasMipmaps()) {
        texture->generateMipmap();
        texture->setTexParameters(Texture2D::TexParams(
            backend::SamplerFilter::LINEAR, backend::SamplerFilter::LINEAR_MIPMAP_LINEAR,
            backend::SamplerAddressMode::CLAMP_TO_EDGE, backend::SamplerAddressMode::CLAMP_TO_EDGE));
    }

    const float width = made->getContentSize().width;
    if (width > 0.0f)
        made->setScale(square / width);
    return made;
}

Sprite* sprite(chess::Piece piece, float square) {
    return sprite(pieces(), piece, square);
}

// --- the board ---------------------------------------------------------------------

float frameWidth(float square) {
    return std::max(4.0f, square * 0.07f);
}

void drawBoard(DrawNode* node, const BoardStyle& style, const Vec2& origin, float square, int columns,
               int rows, float frame) {
    const Vec2 size(square * static_cast<float>(columns), square * static_cast<float>(rows));

    // The frame, so the board has an edge against the background.
    shapes::roundedRect(node, origin - Vec2(frame, frame), size + Vec2(2.0f * frame, 2.0f * frame),
                        frame * style.corner, style.frame);
    if (style.trim.a > 0.0f) {
        const float line = std::max(shapes::pixel(), std::round(frame * 0.3f));
        shapes::rect(node, origin - Vec2(line, line), size + Vec2(2.0f * line, 2.0f * line), style.trim);
    }

    for (int row = 0; row < rows; ++row) {
        for (int column = 0; column < columns; ++column) {
            const bool light = (row + column) % 2 != 0;
            shapes::rect(node, origin + Vec2(static_cast<float>(column) * square, static_cast<float>(row) * square),
                         Vec2(square, square), light ? style.light : style.dark);
        }
    }

    if (!style.tiles)
        return;

    // Each square a tile: lit along its top and left edges, in shadow along
    // the other two, as if it stood a little proud of its neighbours.
    const float edge   = std::max(shapes::pixel(), std::round(square * 0.02f));
    const Color4F lit  = rgb(255, 255, 255, 0.13f);
    const Color4F dim  = rgb(0, 0, 0, 0.22f);
    for (int row = 0; row < rows; ++row) {
        for (int column = 0; column < columns; ++column) {
            const Vec2 at = origin + Vec2(static_cast<float>(column) * square, static_cast<float>(row) * square);
            shapes::rect(node, at + Vec2(0.0f, square - edge), Vec2(square, edge), lit);      // top
            shapes::rect(node, at, Vec2(edge, square - edge), lit);                            // left
            shapes::rect(node, at, Vec2(square, edge), dim);                                   // bottom
            shapes::rect(node, at + Vec2(square - edge, edge), Vec2(edge, square - edge), dim);  // right
        }
    }

    // And a stud wherever four of them meet.
    const float stud = std::max(1.5f, square * 0.036f);
    for (int row = 1; row < rows; ++row) {
        for (int column = 1; column < columns; ++column) {
            const Vec2 at = origin + Vec2(static_cast<float>(column) * square, static_cast<float>(row) * square);
            shapes::circle(node, at, stud + std::max(1.0f, stud * 0.45f), rgb(0, 0, 0, 0.5f));
            shapes::circle(node, at, stud, style.trim);
        }
    }
}

}  // namespace look
