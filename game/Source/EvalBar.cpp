#include "EvalBar.h"

#include "Shapes.h"
#include "Text.h"
#include "Theme.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

using namespace ax;

namespace {

const Color4F kWhiteSide = theme::rgb(238, 238, 232);
const Color4F kBlackSide = theme::rgb(40, 40, 44);
const Color3B kOnWhite(70, 70, 76);
const Color3B kOnBlack(206, 206, 212);

// How fast the boundary closes on its target: the fraction of the remaining
// distance covered per second, as an exponent. Quick enough to follow the
// engine as it changes its mind, slow enough to be seen moving.
constexpr float kFollow = 9.0f;

}  // namespace

EvalBar* EvalBar::create() {
    auto* node = new EvalBar();
    if (node->init()) {
        node->autorelease();
        return node;
    }
    delete node;
    return nullptr;
}

bool EvalBar::init() {
    if (!Node::init())
        return false;
    _bar = DrawNode::create();
    addChild(_bar);
    _label = typeset::label("", 10.5f, kOnWhite);
    addChild(_label);
    return true;
}

float EvalBar::whiteShare(const Score& score) {
    switch (score.kind) {
    case Score::Kind::Unknown:
    case Score::Kind::Drawn:
        return 0.5f;
    case Score::Kind::WhiteWon:
        return 1.0f;
    case Score::Kind::BlackWon:
        return 0.0f;
    case Score::Kind::Mate:
        return score.value > 0 ? 1.0f : 0.0f;
    case Score::Kind::Centipawns: {
        // A pawn up is about 59%, three pawns 75%, five 86%. However large the
        // number, a sliver of the other colour stays until it is actually mate:
        // being a queen up is not the same as having won.
        const float share = 1.0f / (1.0f + std::exp(-0.00368208f * static_cast<float>(score.value)));
        return std::clamp(share, 0.04f, 0.96f);
    }
    }
    return 0.5f;
}

std::string EvalBar::label(const Score& score) {
    char text[16] = "";
    switch (score.kind) {
    case Score::Kind::Unknown:
        break;
    case Score::Kind::Centipawns: {
        const double pawns = std::abs(score.value) / 100.0;
        // One decimal while it means something; past ten pawns nobody is
        // counting tenths.
        std::snprintf(text, sizeof(text), pawns < 9.95 ? "%.1f" : "%.0f", pawns);
        break;
    }
    case Score::Kind::Mate:
        std::snprintf(text, sizeof(text), "#%d", std::abs(score.value));
        break;
    case Score::Kind::WhiteWon:
        return "1–0";
    case Score::Kind::BlackWon:
        return "0–1";
    case Score::Kind::Drawn:
        return "½";
    }
    return text;
}

void EvalBar::setFrame(const Vec2& origin, const Vec2& size) {
    setPosition(origin);
    _size = size;
    redraw();
}

void EvalBar::setFlipped(bool flipped) {
    if (flipped == _flipped)
        return;
    _flipped = flipped;
    redraw();
}

void EvalBar::show(const Score& score) {
    if (score == _score)
        return;
    _score  = score;
    _target = whiteShare(score);
    redraw();
}

bool EvalBar::step(float delta) {
    const float gap = _target - _shown;
    if (std::abs(gap) < 0.002f) {
        if (_shown != _target) {
            _shown = _target;
            redraw();
        }
        return false;
    }
    _shown += gap * (1.0f - std::exp(-kFollow * delta));
    redraw();
    return true;
}

void EvalBar::redraw() {
    _bar->clear();
    if (_size.x <= 0.0f || _size.y <= 0.0f)
        return;

    // A frame, black's colour all the way up, then white's share over it from
    // white's end.
    shapes::roundedRect(_bar, Vec2(-1.0f, -1.0f), Vec2(_size.x + 2.0f, _size.y + 2.0f), 4.0f, theme::kLine);
    shapes::rect(_bar, Vec2::ZERO, _size, kBlackSide);

    const float white = std::round(_size.y * _shown);
    if (white > 0.0f)
        shapes::rect(_bar, Vec2(0.0f, _flipped ? _size.y - white : 0.0f), Vec2(_size.x, white), kWhiteSide);

    // A tick at the middle: the eye needs to know where "level" is to read
    // how far from it the boundary has moved.
    shapes::rect(_bar, Vec2(0.0f, std::round(_size.y * 0.5f) - 1.0f), Vec2(_size.x, 2.0f),
                 theme::rgb(190, 70, 60, 0.85f));

    // The number goes at the end belonging to whoever is ahead, in the ink
    // that shows on that colour. A level game is written at white's end.
    const bool blackAhead = _score.kind == Score::Kind::BlackWon ||
                            ((_score.kind == Score::Kind::Centipawns || _score.kind == Score::Kind::Mate) &&
                             _score.value < 0);
    const std::string text = label(_score);
    if (_label->getString() != text)
        _label->setString(text);
    _label->setVisible(!text.empty());
    _label->setColor(blackAhead ? kOnBlack : kOnWhite);

    const bool atTop = blackAhead != _flipped;  // black's end is the top unless the board is turned
    _label->setAnchorPoint(Vec2(0.5f, atTop ? 1.0f : 0.0f));
    _label->setPosition(Vec2(_size.x * 0.5f, atTop ? _size.y - 5.0f : 5.0f));
}
