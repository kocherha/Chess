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
        // Both are drawn for this game. The Staunton pattern as it comes off
        // a lathe, in boxwood and ebony; see art/classic.py.
        {"classic", text::kPiecesClassic, "pieces/classic"},
        // The same pieces as a chess book prints them: flat, black and white,
        // an outline and a few lines inside it; see art/diagram.py.
        {"diagram", text::kPiecesDiagram, "pieces/diagram"},
        // The same six pieces as a mason would build them; see art/gothic.py.
        {"gothic", text::kPiecesGothic, "pieces/gothic"},
        // And as an animator would draw them: the same six silhouettes in
        // flat colour, each with a face of its own; see art/anime.py.
        {"anime", text::kPiecesAnime, "pieces/anime"},
        // And as they were before Staunton: turned bone, one side stained
        // red, strung like beads, yellowed and cracked; see art/bone.py.
        {"bone", text::kPiecesBone, "pieces/bone"},
        // And as they were before they were chess at all: chaturanga, the
        // four arms of an Indian army and its king — foot soldier, horseman,
        // elephant, chariot, counsellor, raja; see art/ancient.py.
        {"ancient", text::kPiecesAncient, "pieces/ancient"},
        // And as an old console would have drawn them: on a grid of 32 x 32,
        // five tones a side and one dark line; see art/pixel.py.
        {"pixel", text::kPiecesPixel, "pieces/pixel"},
    };
    return sets;
}

const std::vector<BoardStyle>& boards() {
    static const std::vector<BoardStyle> styles = {
        // A club board: maple and walnut veneer in a dark frame, with one
        // thread of boxwood let in between the frame and the field.
        {"wood", text::kBoardWood,
         rgb(225, 197, 148), rgb(158, 105, 66), rgb(52, 32, 21), rgb(206, 170, 112), 0.35f, "boards/wood",
         rgb(205, 210, 60, 0.42f), rgb(60, 130, 60, 0.55f), rgb(30, 80, 30, 0.38f), rgb(226, 48, 40, 0.85f)},

        // A church floor: limestone and oxblood flags, worn at the edges, a
        // brass stud where four of them meet, in a frame of black iron. The
        // marks are candle colours: green would be lost on the red, and red
        // on the red would be no warning at all.
        {"gothic", text::kBoardGothic,
         rgb(190, 183, 167), rgb(93, 33, 39), rgb(16, 13, 17), rgb(158, 124, 66), 0.15f, "boards/gothic",
         rgb(226, 176, 60, 0.42f), rgb(236, 196, 84, 0.72f), rgb(18, 10, 12, 0.46f), rgb(255, 150, 40, 0.92f)},

        // Cool grey: the least colour of the four, for pieces to stand out on.
        {"marble", text::kBoardMarble,
         rgb(228, 230, 234), rgb(141, 150, 167), rgb(46, 50, 60), kNone, 1.0f, nullptr,
         rgb(120, 170, 230, 0.45f), rgb(60, 130, 200, 0.55f), rgb(24, 40, 70, 0.36f), rgb(226, 48, 40, 0.85f)},

        // The green baize of a tournament hall.
        {"emerald", text::kBoardEmerald,
         rgb(235, 236, 208), rgb(115, 149, 82), rgb(38, 50, 32), kNone, 1.0f, nullptr,
         rgb(245, 246, 105, 0.52f), rgb(250, 200, 60, 0.62f), rgb(20, 40, 20, 0.34f), rgb(226, 48, 40, 0.85f)},

        // Cream and cherry-blossom pink, flat as a cel, with fallen petals.
        // Pink leaves little room for a red warning, so the check here is a
        // deep crimson, and the marks are the colours pink is not: gold and
        // teal.
        {"sakura", text::kBoardSakura,
         rgb(254, 244, 239), rgb(244, 171, 192), rgb(168, 84, 120), rgb(255, 238, 244), 1.0f, "boards/sakura",
         rgb(255, 204, 70, 0.52f), rgb(60, 178, 190, 0.6f), rgb(120, 46, 92, 0.34f), rgb(214, 20, 70, 0.9f)},

        // An old tooled board: vellum and dark calf, each square ruled round,
        // foxed and cracked, in a frame of the same calf with a gilt line.
        {"leather", text::kBoardLeather,
         rgb(200, 180, 135), rgb(87, 56, 35), rgb(40, 25, 16), rgb(182, 146, 78), 0.5f, "boards/leather",
         rgb(214, 196, 64, 0.44f), rgb(70, 136, 84, 0.58f), rgb(30, 50, 26, 0.4f), rgb(226, 48, 40, 0.85f)},

        // Older still: slabs of buff and red sandstone, pitted and chipped,
        // set in earth. Red would be lost on it, so the marks are a painter's
        // other colours: verdigris for the last move, yellow for the choice.
        {"stone", text::kBoardStone,
         rgb(182, 151, 104), rgb(127, 66, 47), rgb(44, 28, 20), rgb(150, 110, 68), 0.2f, "boards/stone",
         rgb(70, 176, 150, 0.44f), rgb(244, 206, 70, 0.62f), rgb(26, 30, 24, 0.44f), rgb(255, 96, 30, 0.95f)},

        // Tiles of 32 x 32 pixels, cream and green, on the same grid as the
        // pixel pieces; a square frame, as nothing on that screen was round.
        {"pixel", text::kBoardPixel,
         rgb(228, 216, 176), rgb(102, 145, 95), rgb(40, 32, 52), rgb(244, 235, 203), 0.0f, "boards/pixel",
         rgb(250, 214, 70, 0.5f), rgb(70, 170, 250, 0.58f), rgb(30, 24, 44, 0.42f), rgb(232, 52, 52, 0.9f)},
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

namespace {

// The artwork is far larger than a square on screen, and scaled down with
// plain bilinear filtering its thin lines shimmer. Mipmaps fix that; a
// texture is shared by every sprite cut from it, so this only does work the
// first time.
void smooth(Sprite* sprite) {
    Texture2D* texture = sprite->getTexture();
    if (texture != nullptr && !texture->hasMipmaps()) {
        texture->generateMipmap();
        texture->setTexParameters(Texture2D::TexParams(
            backend::SamplerFilter::LINEAR, backend::SamplerFilter::LINEAR_MIPMAP_LINEAR,
            backend::SamplerAddressMode::CLAMP_TO_EDGE, backend::SamplerAddressMode::CLAMP_TO_EDGE));
    }
}

}  // namespace

Sprite* sprite(const PieceSet& set, chess::Piece piece, float square) {
    std::string name = set.folder;
    name += '/';
    name += piece.color == chess::Color::White ? 'w' : 'b';
    name += chess::pieceLetter(piece.type);
    name += ".png";

    auto* made = Sprite::create(name);
    if (made == nullptr)
        return nullptr;
    smooth(made);

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

namespace {

// One square of a board laid from pictures: one of the four cuts in the
// picture, perhaps turned over. Which, is decided by where the square is and
// nothing else, so the board is the same board every time it is drawn.
Sprite* tile(const BoardStyle& style, bool light, int column, int row, float square) {
    auto* made = Sprite::create(std::string(style.tiles) + (light ? "/light.png" : "/dark.png"));
    if (made == nullptr)
        return nullptr;
    smooth(made);

    const unsigned pick = static_cast<unsigned>(column * 73 + row * 151 + column * row * 31) % 16u;
    const Vec2 half(made->getContentSize().width * 0.5f, made->getContentSize().height * 0.5f);
    // A hair inside its quarter, so that a neighbouring cut does not bleed in
    // along the edge when the picture is reduced.
    const float inset = half.x / 256.0f;
    made->setTextureRect(Rect(static_cast<float>(pick & 1u) * half.x + inset,
                              static_cast<float>((pick >> 1) & 1u) * half.y + inset, half.x - 2.0f * inset,
                              half.y - 2.0f * inset));
    made->setFlippedX((pick & 4u) != 0);
    made->setFlippedY((pick & 8u) != 0);
    made->setAnchorPoint(Vec2::ZERO);
    made->setScale(square / (half.x - 2.0f * inset), square / (half.y - 2.0f * inset));
    return made;
}

}  // namespace

void drawBoard(DrawNode* node, Node* tiles, const BoardStyle& style, const Vec2& origin, float square,
               int columns, int rows, float frame) {
    const Vec2 size(square * static_cast<float>(columns), square * static_cast<float>(rows));
    tiles->removeAllChildren();

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
            const Vec2 at    = origin + Vec2(static_cast<float>(column) * square, static_cast<float>(row) * square);
            // The flat colour is drawn even under a picture: it is what shows
            // if the picture is missing.
            shapes::rect(node, at, Vec2(square, square), light ? style.light : style.dark);
            if (style.tiles == nullptr)
                continue;
            if (Sprite* made = tile(style, light, column, row, square)) {
                made->setPosition(at);
                tiles->addChild(made);
            }
        }
    }
}

}  // namespace look
