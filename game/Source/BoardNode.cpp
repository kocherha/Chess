#include "BoardNode.h"

#include "Look.h"
#include "Shapes.h"
#include "Text.h"
#include "Theme.h"

#include <algorithm>
#include <string>

using namespace ax;
using namespace chess;

namespace {

constexpr float kSlideSeconds = 0.14f;

// The order the promotion column offers its pieces in: the queen first,
// because it is what is wanted nine times in ten, on the square the pawn
// reached.
constexpr PieceType kPromotionOrder[4] = {PieceType::Queen, PieceType::Knight, PieceType::Rook,
                                          PieceType::Bishop};

bool lightSquare(Square square) {
    return (fileOf(square) + rankOf(square)) % 2 != 0;
}

// The two badges that are pictures rather than characters. Both are drawn in
// white inside a disc of `radius` around `at`.
void drawCrown(DrawNode* node, const Vec2& at, float radius) {
    const Color4F white = Color4F::WHITE;
    const auto p = [&](float x, float y) { return at + Vec2(x, y) * radius; };
    // A band along the bottom and three points standing on it, the outer two
    // leaning out.
    shapes::convex(node, {p(-0.50f, -0.46f), p(0.50f, -0.46f), p(0.50f, -0.16f), p(-0.50f, -0.16f)}, white);
    shapes::convex(node, {p(-0.50f, -0.30f), p(-0.04f, -0.30f), p(-0.64f, 0.38f)}, white);
    shapes::convex(node, {p(-0.34f, -0.30f), p(0.34f, -0.30f), p(0.0f, 0.54f)}, white);
    shapes::convex(node, {p(0.04f, -0.30f), p(0.50f, -0.30f), p(0.64f, 0.38f)}, white);
}

void drawFlag(DrawNode* node, const Vec2& at, float radius) {
    const Color4F white = Color4F::WHITE;
    const auto p = [&](float x, float y) { return at + Vec2(x, y) * radius; };
    shapes::convex(node, {p(-0.40f, -0.56f), p(-0.24f, -0.56f), p(-0.24f, 0.56f), p(-0.40f, 0.56f)}, white);
    shapes::convex(node, {p(-0.30f, 0.56f), p(-0.30f, -0.06f), p(0.52f, 0.25f)}, white);
}

}  // namespace

BoardNode* BoardNode::create() {
    auto* node = new BoardNode();
    if (node->init()) {
        node->autorelease();
        return node;
    }
    delete node;
    return nullptr;
}

bool BoardNode::init() {
    if (!Node::init())
        return false;

    _squares = DrawNode::create();
    _labels  = Node::create();
    _under   = DrawNode::create();
    _pieces  = Node::create();
    _over    = DrawNode::create();
    addChild(_squares, 0);
    addChild(_labels, 1);
    addChild(_under, 2);
    addChild(_pieces, 3);
    addChild(_over, 4);

    for (Label*& sign : _signs) {
        sign = Label::createWithSystemFont("", theme::kFont, 12.0f);
        typeset::embolden(sign);
        sign->setVisible(false);
        addChild(sign, 5);
    }

    redrawSquares();
    return true;
}

// --- geometry ----------------------------------------------------------------------

Vec2 BoardNode::origin(Square square) const {
    const int column = _flipped ? 7 - fileOf(square) : fileOf(square);
    const int row    = _flipped ? 7 - rankOf(square) : rankOf(square);
    const float s    = squareSize();
    return Vec2(static_cast<float>(column) * s, static_cast<float>(row) * s);
}

Vec2 BoardNode::centre(Square square) const {
    const float half = squareSize() * 0.5f;
    return origin(square) + Vec2(half, half);
}

Square BoardNode::squareAt(const Vec2& worldPoint) const {
    const Vec2 local = convertToNodeSpace(worldPoint);
    if (local.x < 0.0f || local.y < 0.0f || local.x >= _side || local.y >= _side)
        return kNoSquare;
    const int column = std::min(7, static_cast<int>(local.x / squareSize()));
    const int row    = std::min(7, static_cast<int>(local.y / squareSize()));
    return makeSquare(_flipped ? 7 - column : column, _flipped ? 7 - row : row);
}

void BoardNode::setSide(float side) {
    if (side == _side)
        return;
    _side = side;
    setContentSize(Size(side, side));
    redrawSquares();
    show(_shown);
    if (_promotion != nullptr) {
        const Square target = _promotionTarget;
        const Color colour  = rankOf(target) == 7 ? Color::White : Color::Black;
        openPromotion(target, colour);
    }
}

void BoardNode::setFlipped(bool flipped) {
    if (flipped == _flipped)
        return;
    _flipped = flipped;
    redrawSquares();
    show(_shown);
}

void BoardNode::restyle() {
    redrawSquares();
    show(_shown);
}

// --- the board itself --------------------------------------------------------------

void BoardNode::redrawSquares() {
    _squares->clear();
    _labels->removeAllChildren();

    const float s                 = squareSize();
    const look::BoardStyle& style = look::board();

    look::drawBoard(_squares, style, Vec2::ZERO, s, 8, 8, look::frameWidth(s));

    // Coordinates live inside the edge squares, in the colour of the other
    // kind of square: letters along the bottom, numbers up the left side.
    const float points = std::clamp(s * 0.17f, 9.0f, 15.0f);
    const float inset  = std::max(2.0f, s * 0.05f);
    const auto tint = [&style](Square square) {
        const Color4F& c = lightSquare(square) ? style.dark : style.light;
        return Color3B(static_cast<uint8_t>(c.r * 255.0f), static_cast<uint8_t>(c.g * 255.0f),
                       static_cast<uint8_t>(c.b * 255.0f));
    };

    for (int i = 0; i < 8; ++i) {
        // The square in screen column i of the bottom row, and row i of the
        // left column — whichever squares those are with the board this way up.
        const Square bottom = makeSquare(_flipped ? 7 - i : i, _flipped ? 7 : 0);
        const Square left   = makeSquare(_flipped ? 7 : 0, _flipped ? 7 - i : i);

        auto* letter = Label::createWithSystemFont(std::string(1, static_cast<char>('a' + fileOf(bottom))),
                                                   theme::kFont, points);
        letter->setAnchorPoint(Vec2(1.0f, 0.0f));
        letter->setPosition(origin(bottom) + Vec2(s - inset, inset * 0.5f));
        letter->setColor(tint(bottom));
        _labels->addChild(letter);

        auto* number = Label::createWithSystemFont(std::string(1, static_cast<char>('1' + rankOf(left))),
                                                   theme::kFont, points);
        number->setAnchorPoint(Vec2(0.0f, 1.0f));
        number->setPosition(origin(left) + Vec2(inset, s - inset * 0.5f));
        number->setColor(tint(left));
        _labels->addChild(number);
    }

    redrawMarks();
}

// --- pieces ------------------------------------------------------------------------

Sprite* BoardNode::makePiece(Piece piece) const {
    return look::sprite(piece, squareSize());
}

void BoardNode::slide(Node* piece, const Vec2& from, const Vec2& to) {
    piece->setPosition(from);
    piece->setLocalZOrder(1);  // above the pieces it passes
    ++_animations;
    piece->runAction(Sequence::create(EaseSineOut::create(MoveTo::create(kSlideSeconds, to)),
                                      CallFunc::create([this, piece] {
                                          piece->setLocalZOrder(0);
                                          --_animations;
                                      }),
                                      nullptr));
}

void BoardNode::show(const Position& position, const Move* played, bool slidePiece) {
    const Position before = _shown;
    _shown                = position;

    // Anything still in flight belongs to the picture being replaced.
    _pieces->removeAllChildren();
    _sprites.fill(nullptr);
    _dragged    = nullptr;
    _dragSquare = kNoSquare;
    _animations = 0;

    for (Square square = 0; square < 64; ++square) {
        const Piece piece = position.at(square);
        if (piece.empty())
            continue;
        Sprite* sprite = makePiece(piece);
        if (sprite == nullptr)
            continue;
        sprite->setPosition(centre(square));
        _pieces->addChild(sprite, 0);
        _sprites[static_cast<std::size_t>(square)] = sprite;
    }

    if (played == nullptr)
        return;

    // What was taken stays visible for as long as the capture takes to happen.
    if (played->isCapture()) {
        const Square victim = played->enPassant
                                  ? makeSquare(fileOf(played->to), rankOf(played->from))
                                  : played->to;
        if (Sprite* ghost = before.at(victim).empty() ? nullptr : makePiece(before.at(victim))) {
            ghost->setPosition(centre(victim));
            _pieces->addChild(ghost, -1);
            ghost->runAction(Sequence::create(FadeOut::create(kSlideSeconds), RemoveSelf::create(), nullptr));
        }
    }

    if (slidePiece) {
        if (Sprite* mover = _sprites[static_cast<std::size_t>(played->to)])
            slide(mover, centre(played->from), centre(played->to));
    }

    // The rook's half of castling is never carried by hand, so it always slides.
    if (played->isCastle()) {
        const int rank        = rankOf(played->from);
        const Square rookFrom = makeSquare(played->castleKingSide ? 7 : 0, rank);
        const Square rookTo   = makeSquare(played->castleKingSide ? 5 : 3, rank);
        if (Sprite* rook = _sprites[static_cast<std::size_t>(rookTo)])
            slide(rook, centre(rookFrom), centre(rookTo));
    }
}

// --- marks -------------------------------------------------------------------------

void BoardNode::setMarks(const Marks& marks) {
    _marks = marks;
    redrawMarks();
}

void BoardNode::redrawMarks() {
    _under->clear();
    _over->clear();

    const float s = squareSize();
    const Vec2 cell(s, s);
    const look::BoardStyle& style = look::board();

    for (Square square : {_marks.lastFrom, _marks.lastTo}) {
        if (square != kNoSquare)
            shapes::rect(_under, origin(square), cell, style.lastMove);
    }

    if (_marks.selected != kNoSquare)
        shapes::rect(_under, origin(_marks.selected), cell, style.selected);

    if (_marks.check != kNoSquare)
        shapes::glow(_under, centre(_marks.check), s * 0.62f, style.check);

    // Where the selected piece can go: a dot on an empty square, a ring
    // around something it would capture. A promotion appears four times in
    // the list, once per piece; it is one square and gets one mark.
    Square marked[64];
    int count = 0;
    for (const Move& move : _marks.targets) {
        if (std::find(marked, marked + count, move.to) != marked + count)
            continue;
        marked[count++] = move.to;

        if (move.isCapture() && !move.enPassant)
            shapes::ring(_under, centre(move.to), s * 0.39f, s * 0.47f, style.target);
        else
            shapes::circle(_under, centre(move.to), s * 0.15f, style.target);
    }

    // Badges: a small disc in the top right corner of a square, over the
    // piece, with a sign on it. One for what the last move was worth, and one
    // for each king the state of the game says something about.
    const float radius = std::max(5.0f, s * 0.125f);
    const float inset  = radius + std::max(1.0f, s * 0.03f);
    const auto corner = [&](Square square, bool left) {
        return origin(square) + Vec2(left ? inset : s - inset, s - inset);
    };

    std::size_t written = 0;
    for (Label* sign : _signs)
        sign->setVisible(false);
    const auto write = [&](const Vec2& at, const char* sign) {
        Label* label = _signs[written++];
        label->setString(sign);
        label->setSystemFontSize(radius * 1.3f);
        // Back by half of what the bold drawing adds on the right.
        label->setPosition(at - Vec2(typeset::kBoldShift * 0.5f, 0.0f));
        label->setVisible(true);
    };

    bool onLastMove = false;
    for (const KingSign& king : _marks.kings) {
        if (king.sign == Sign::None || king.square == kNoSquare)
            continue;
        onLastMove    = onLastMove || king.square == _marks.lastTo;
        const Vec2 at = corner(king.square, false);
        switch (king.sign) {
        case Sign::Check:
            shapes::circle(_over, at, radius, theme::kSignDanger);
            write(at, "+");
            break;
        case Sign::Mate:
            shapes::circle(_over, at, radius, theme::kSignDanger);
            write(at, "#");
            break;
        case Sign::Won:
            shapes::circle(_over, at, radius, theme::kSignWon);
            drawCrown(_over, at, radius);
            break;
        case Sign::Resigned:
            shapes::circle(_over, at, radius, theme::kSignQuiet);
            drawFlag(_over, at, radius);
            break;
        case Sign::Draw:
            shapes::circle(_over, at, radius, theme::kSignQuiet);
            write(at, "½");
            break;
        case Sign::None:
            break;
        }
    }

    if (_marks.verdict != Mark::None && _marks.lastTo != kNoSquare) {
        const Vec2 at = corner(_marks.lastTo, onLastMove);
        shapes::circle(_over, at, radius, theme::markColour(_marks.verdict));
        write(at, suffix(_marks.verdict));
    }

    if (_marks.hintFrom != kNoSquare && _marks.hintTo != kNoSquare) {
        // Start a little way out from the centre so the piece being pointed
        // at is not buried under the arrow's tail.
        const Vec2 from = centre(_marks.hintFrom), to = centre(_marks.hintTo);
        const Vec2 unit = (to - from).getNormalized();
        shapes::arrow(_over, from + unit * (s * 0.28f), to, s * 0.17f, theme::kHint);
    }
}

// --- dragging ----------------------------------------------------------------------

void BoardNode::beginDrag(Square square, const Vec2& worldPoint) {
    endDrag();
    if (square < 0 || square > 63)
        return;
    Sprite* sprite = _sprites[static_cast<std::size_t>(square)];
    if (sprite == nullptr)
        return;

    _dragged    = sprite;
    _dragSquare = square;
    sprite->stopAllActions();
    sprite->setLocalZOrder(2);
    // Lifted pieces are a touch larger: it reads as "in the hand".
    const float width = sprite->getContentSize().width;
    if (width > 0.0f)
        sprite->setScale(squareSize() * 1.12f / width);
    dragTo(worldPoint);
}

void BoardNode::dragTo(const Vec2& worldPoint) {
    if (_dragged == nullptr)
        return;
    Vec2 local = convertToNodeSpace(worldPoint);
    // The piece may leave the board a little, but not wander off across the
    // window — it should stay obvious what it belongs to.
    const float slack = squareSize() * 0.4f;
    local.x = std::clamp(local.x, -slack, _side + slack);
    local.y = std::clamp(local.y, -slack, _side + slack);
    _dragged->setPosition(local);
}

void BoardNode::endDrag() {
    if (_dragged == nullptr)
        return;
    const float width = _dragged->getContentSize().width;
    if (width > 0.0f)
        _dragged->setScale(squareSize() / width);
    _dragged->setLocalZOrder(0);
    _dragged->setPosition(centre(_dragSquare));
    _dragged    = nullptr;
    _dragSquare = kNoSquare;
}

// --- promotion ---------------------------------------------------------------------

void BoardNode::openPromotion(Square target, Color colour) {
    closePromotion();

    _promotionTarget = target;
    _promotion       = Node::create();
    addChild(_promotion, 6);

    const float s = squareSize();

    // The rest of the board steps back.
    auto* back = DrawNode::create();
    _promotion->addChild(back);
    shapes::rect(back, Vec2::ZERO, Vec2(_side, _side), theme::rgb(0, 0, 0, 0.45f));

    // The column runs away from the edge the pawn has just reached, so it is
    // always on the board whichever way up that is.
    const Vec2 top = origin(target);
    _promotionDown = top.y > _side * 0.5f;

    for (int i = 0; i < 4; ++i) {
        const Vec2 cell = top + Vec2(0.0f, (_promotionDown ? -1.0f : 1.0f) * static_cast<float>(i) * s);
        shapes::roundedRect(back, cell + Vec2(2.0f, 2.0f), Vec2(s - 4.0f, s - 4.0f), s * 0.12f,
                            theme::rgb(250, 248, 244), 1.5f, theme::rgb(118, 168, 72));
        if (Sprite* sprite = makePiece(Piece{kPromotionOrder[i], colour})) {
            sprite->setPosition(cell + Vec2(s * 0.5f, s * 0.5f));
            sprite->setScale(sprite->getScale() * 0.9f);
            _promotion->addChild(sprite);
        }
    }
}

void BoardNode::closePromotion() {
    if (_promotion == nullptr)
        return;
    _promotion->removeFromParent();
    _promotion       = nullptr;
    _promotionTarget = kNoSquare;
}

PieceType BoardNode::promotionAt(const Vec2& worldPoint) const {
    if (_promotion == nullptr)
        return PieceType::None;

    const float s    = squareSize();
    const Vec2 local = convertToNodeSpace(worldPoint);
    const Vec2 top   = origin(_promotionTarget);
    if (local.x < top.x || local.x >= top.x + s)
        return PieceType::None;

    // How many cells from the first one, counted in the direction the column runs.
    const float along = _promotionDown ? (top.y + s - local.y) : (local.y - top.y);
    if (along < 0.0f || along >= 4.0f * s)
        return PieceType::None;
    return kPromotionOrder[static_cast<int>(along / s)];
}
