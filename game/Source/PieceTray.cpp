#include "PieceTray.h"

#include "Look.h"
#include "Shapes.h"
#include "Theme.h"

#include <algorithm>
#include <cmath>

using namespace ax;
using namespace chess;

namespace {

constexpr PieceType kOrder[6] = {PieceType::King, PieceType::Queen, PieceType::Rook,
                                 PieceType::Bishop, PieceType::Knight, PieceType::Pawn};

}  // namespace

PieceTray* PieceTray::create(Color colour) {
    auto* node = new PieceTray();
    if (node->initWith(colour)) {
        node->autorelease();
        return node;
    }
    delete node;
    return nullptr;
}

bool PieceTray::initWith(Color colour) {
    if (!Node::init())
        return false;
    _colour = colour;
    _back   = DrawNode::create();
    _pieces = Node::create();
    addChild(_back);
    addChild(_pieces);
    return true;
}

void PieceTray::setFrame(const Vec2& origin, const Vec2& size) {
    setPosition(origin);
    _size = size;
    _cell = std::floor(std::min(size.y, size.x / static_cast<float>(kCells)));
    rebuild();
}

void PieceTray::restyle() {
    rebuild();
}

void PieceTray::setChosen(const Item& item) {
    if (item == _chosen)
        return;
    _chosen = item;
    rebuild();
}

PieceTray::Item PieceTray::itemAt(int index) const {
    Item item;
    if (index == kCells - 1)
        item.eraser = true;
    else if (index >= 0 && index < kCells - 1)
        item.piece = Piece{kOrder[index], _colour};
    return item;
}

Vec2 PieceTray::cellOrigin(int index) const {
    const float row = _cell * static_cast<float>(kCells);
    return Vec2(std::floor((_size.x - row) * 0.5f) + _cell * static_cast<float>(index),
                std::floor((_size.y - _cell) * 0.5f));
}

PieceTray::Item PieceTray::at(const Vec2& worldPoint) const {
    if (!isVisible() || _cell <= 0.0f)
        return {};
    const Vec2 local = convertToNodeSpace(worldPoint);
    for (int i = 0; i < kCells; ++i) {
        const Vec2 corner = cellOrigin(i);
        if (local.x >= corner.x && local.x < corner.x + _cell && local.y >= corner.y && local.y < corner.y + _cell)
            return itemAt(i);
    }
    return {};
}

void PieceTray::rebuild() {
    _back->clear();
    _pieces->removeAllChildren();
    if (_cell <= 0.0f)
        return;

    // The same grey the captured pieces lie on: both colours show on it.
    const Vec2 start = cellOrigin(0);
    shapes::roundedRect(_back, start, Vec2(_cell * kCells, _cell), _cell * 0.16f, theme::kCard, 1.0f, theme::kLine);

    for (int i = 0; i < kCells; ++i) {
        const Item item   = itemAt(i);
        const Vec2 corner = cellOrigin(i);
        const Vec2 centre = corner + Vec2(_cell * 0.5f, _cell * 0.5f);

        if (item == _chosen)
            shapes::roundedRect(_back, corner + Vec2(2.0f, 2.0f), Vec2(_cell - 4.0f, _cell - 4.0f), _cell * 0.14f,
                                theme::kTray, 2.0f, theme::kAccent);

        if (item.eraser) {
            // A cross: two bars, corner to corner.
            const float arm = _cell * 0.22f, half = std::max(1.5f, _cell * 0.045f);
            for (const float sign : {1.0f, -1.0f}) {
                const Vec2 along = Vec2(arm, sign * arm);
                const Vec2 side  = Vec2(-sign * half, half) * (1.0f / std::sqrt(2.0f)) * 2.0f;
                shapes::convex(_back, {centre - along - side, centre + along - side, centre + along + side,
                                       centre - along + side},
                               theme::rgb(214, 96, 84));
            }
            continue;
        }

        if (Sprite* sprite = look::sprite(item.piece, _cell * 0.92f)) {
            sprite->setPosition(centre);
            _pieces->addChild(sprite);
        }
    }
}
