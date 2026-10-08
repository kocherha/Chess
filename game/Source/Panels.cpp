#include "Panels.h"

#include "Look.h"
#include "Shapes.h"
#include "Strings.h"
#include "Text.h"
#include "Theme.h"

#include <algorithm>

using namespace ax;
using namespace chess;

namespace {

constexpr float kPadding   = 20.0f;
constexpr float kRowHeight = 26.0f;
constexpr float kButtonHeight = 40.0f;
constexpr float kGap       = 10.0f;

// Small sprites for captured pieces. Same artwork as the board, same texture.
Sprite* trophy(PieceType type, Color colour, float height) {
    return look::sprite(Piece{type, colour}, height);
}

}  // namespace

// --- PlayerBar ---------------------------------------------------------------------

PlayerBar* PlayerBar::create() {
    auto* node = new PlayerBar();
    if (node->init()) {
        node->autorelease();
        return node;
    }
    delete node;
    return nullptr;
}

bool PlayerBar::init() {
    if (!Node::init())
        return false;
    _marker  = DrawNode::create();
    _content = Node::create();
    addChild(_marker);
    addChild(_content);
    return true;
}

void PlayerBar::setFrame(const Vec2& origin, const Vec2& size) {
    setPosition(origin);
    _size = size;
    rebuild();
}

void PlayerBar::set(const std::string& name, Color colour, const std::vector<PieceType>& trophies,
                    const std::string& lead, bool toMove) {
    if (name == _name && colour == _colour && trophies == _trophies && lead == _lead && toMove == _toMove)
        return;
    _name     = name;
    _colour   = colour;
    _trophies = trophies;
    _lead     = lead;
    _toMove   = toMove;
    rebuild();
}

void PlayerBar::restyle() {
    rebuild();
}

void PlayerBar::rebuild() {
    _marker->clear();
    _content->removeAllChildren();
    if (_size.x <= 0.0f)
        return;

    const float middle = _size.y * 0.5f;

    // A disc in the player's colour; a green ring around it when it is their
    // move. It is the first thing the eye finds on returning to the window.
    const float radius = std::min(9.0f, _size.y * 0.3f);
    const Vec2 disc(radius + 2.0f, middle);
    if (_toMove)
        shapes::circle(_marker, disc, radius + 3.5f, theme::kAccent);
    shapes::circle(_marker, disc, radius + 1.0f, theme::kLine);
    shapes::circle(_marker, disc, radius,
                   _colour == Color::White ? theme::rgb(244, 244, 240) : theme::rgb(28, 28, 30));

    float x = disc.x + radius + 12.0f;

    auto* name = typeset::label(_name, 16.0f, _toMove ? theme::kInk : theme::kDim);
    name->setAnchorPoint(Vec2(0.0f, 0.5f));
    name->setPosition(Vec2(x, middle));
    _content->addChild(name);
    x += name->getContentSize().width + 16.0f;

    // Trophies overlap like cards in a hand: fifteen pieces would not fit side
    // by side, and the eye counts a fan of them just as well. They lie on a
    // grey tray, so that black pieces and white ones are equally easy to see.
    if (!_trophies.empty()) {
        const float height = std::min(32.0f, _size.y * 0.86f);
        const float step   = height * 0.46f;
        const float gap    = step * 0.45f;  // between one kind of piece and the next

        // How many fit before the name and the tray run out of bar.
        const float room = _size.x - x - 44.0f;
        std::size_t shown = 0;
        float width       = height;
        for (std::size_t i = 0; i < _trophies.size(); ++i) {
            const float grown = i == 0 ? height
                                       : width + step + (_trophies[i] != _trophies[i - 1] ? gap : 0.0f);
            if (grown > room)
                break;
            width = grown;
            shown = i + 1;
        }

        if (shown > 0) {
            const float pad = 5.0f;
            shapes::roundedRect(_marker, Vec2(x - pad, middle - height * 0.5f), Vec2(width + 2.0f * pad, height),
                                height * 0.28f, theme::kTray);

            const Color theirs = opposite(_colour);
            float at           = x;
            for (std::size_t i = 0; i < shown; ++i) {
                if (i > 0)
                    at += step + (_trophies[i] != _trophies[i - 1] ? gap : 0.0f);
                if (Sprite* sprite = trophy(_trophies[i], theirs, height)) {
                    sprite->setAnchorPoint(Vec2(0.0f, 0.5f));
                    sprite->setPosition(Vec2(at, middle));
                    _content->addChild(sprite);
                }
            }
            x += width + pad + 10.0f;
        }
    }

    if (!_lead.empty()) {
        auto* lead = typeset::label(_lead, 14.0f, theme::kDim);
        lead->setAnchorPoint(Vec2(0.0f, 0.5f));
        lead->setPosition(Vec2(x, middle));
        _content->addChild(lead);
    }
}

// --- SidePanel ---------------------------------------------------------------------

SidePanel* SidePanel::create() {
    auto* node = new SidePanel();
    if (node->init()) {
        node->autorelease();
        return node;
    }
    delete node;
    return nullptr;
}

bool SidePanel::init() {
    if (!Node::init())
        return false;

    _back = DrawNode::create();
    addChild(_back);

    _title = typeset::label(text::kTitle, 26.0f, theme::kInk);
    _title->enableBold();
    _subtitle   = typeset::label("", 14.0f, theme::kDim);
    _headline   = typeset::label("", 19.0f, theme::kInk);
    _detail     = typeset::label("", 14.0f, theme::kDim);
    _movesTitle = typeset::label(text::kMoves, 13.0f, theme::kFaint);
    for (Label* label : {_title, _subtitle, _headline, _detail, _movesTitle}) {
        label->setAnchorPoint(Vec2(0.0f, 0.5f));
        addChild(label);
    }
    _detail->setAnchorPoint(Vec2(0.0f, 1.0f));  // it may wrap, so it hangs from its top

    _moveRows = Node::create();
    addChild(_moveRows);

    looks   = Button::create(text::kLooks, Button::Style::Plain, 14.0f);
    newGame = Button::create(text::kNewGame, Button::Style::Primary, 16.0f);
    undo    = Button::create(text::kUndo);
    hint    = Button::create(text::kHint);
    flip    = Button::create(text::kFlip);
    resign  = Button::create(text::kResign);
    for (Button* button : {looks, newGame, undo, hint, flip, resign})
        addChild(button);

    return true;
}

void SidePanel::setFrame(const Vec2& origin, const Vec2& size) {
    setPosition(origin);
    _size = size;
    layout();
}

bool SidePanel::contains(const Vec2& worldPoint) const {
    const Vec2 local = convertToNodeSpace(worldPoint);
    return local.x >= 0.0f && local.y >= 0.0f && local.x <= _size.x && local.y <= _size.y;
}

void SidePanel::setSubtitle(const std::string& text) {
    _subtitle->setString(text);
}

void SidePanel::setStatus(const std::string& headline, const std::string& detail, Tone tone) {
    if (headline == _headlineText && detail == _detailText && tone == _tone)
        return;
    _headlineText = headline;
    _detailText   = detail;
    _tone         = tone;
    // The card's border follows the tone and its height follows the detail —
    // none, one line, or a sentence that wraps — and everything below the card
    // moves with it. All of that is layout()'s to work out.
    layout();
}

void SidePanel::layout() {
    if (_size.x <= 0.0f)
        return;

    const float width = _size.x - 2.0f * kPadding;
    float y           = _size.y - kPadding;

    _back->clear();
    shapes::rect(_back, Vec2::ZERO, _size, theme::kPanel);
    shapes::rect(_back, Vec2::ZERO, Vec2(1.0f, _size.y), theme::kLine);

    // Title and what kind of game this is. Level with the title, at the far
    // side: the one button that is about the room rather than the game.
    _title->setPosition(Vec2(kPadding, y - 15.0f));
    looks->setFrame(Vec2(kPadding + width - 92.0f, y - 30.0f), Vec2(92.0f, 30.0f));
    y -= 38.0f;
    _subtitle->setPosition(Vec2(kPadding, y - 8.0f));
    y -= 30.0f;

    // The status card: whose move it is, or how it ended. One line sits in
    // the middle of it; a second line goes underneath and may wrap, and the
    // card grows to hold it.
    _headline->setString(_headlineText);
    _headline->setColor(_tone == Tone::Good      ? theme::kGood
                        : _tone == Tone::Warning ? theme::kWarn
                                                 : theme::kInk);
    _detail->setString(typeset::wrap(_detailText, 14.0f, width - 32.0f));

    const bool twoLines    = !_detailText.empty();
    const float cardHeight = twoLines ? std::max(76.0f, 44.0f + _detail->getContentSize().height + 14.0f) : 76.0f;
    shapes::roundedRect(_back, Vec2(kPadding, y - cardHeight), Vec2(width, cardHeight), 10.0f, theme::kCard,
                        1.0f,
                        _tone == Tone::Good      ? theme::kAccent
                        : _tone == Tone::Warning ? theme::rgb(170, 130, 70)
                                                 : theme::kLine);
    _headline->setPosition(Vec2(kPadding + 16.0f, y - (twoLines ? 27.0f : cardHeight * 0.5f)));
    _detail->setPosition(Vec2(kPadding + 16.0f, y - 44.0f));
    y -= cardHeight + 20.0f;

    // Buttons, from the bottom up.
    float bottom = kPadding;
    const float half = (width - kGap) * 0.5f;
    flip->setFrame(Vec2(kPadding, bottom), Vec2(half, kButtonHeight));
    resign->setFrame(Vec2(kPadding + half + kGap, bottom), Vec2(half, kButtonHeight));
    bottom += kButtonHeight + kGap;
    undo->setFrame(Vec2(kPadding, bottom), Vec2(half, kButtonHeight));
    hint->setFrame(Vec2(kPadding + half + kGap, bottom), Vec2(half, kButtonHeight));
    bottom += kButtonHeight + kGap;
    newGame->setFrame(Vec2(kPadding, bottom), Vec2(width, kButtonHeight + 4.0f));
    bottom += kButtonHeight + 4.0f + 18.0f;

    // The move list takes whatever is left between the two.
    _movesTitle->setPosition(Vec2(kPadding, y - 8.0f));
    y -= 24.0f;
    _listTop    = y;
    _listBottom = bottom;
    shapes::rect(_back, Vec2(kPadding, _listTop + 2.0f), Vec2(width, 1.0f), theme::kLine);

    rebuildMoves();
}

int SidePanel::rowCount() const {
    // A game that began with black to move has an empty first cell.
    const std::size_t cells = _moves.size() + (_whiteFirst ? 0 : 1);
    return static_cast<int>((cells + 1) / 2);
}

int SidePanel::visibleRows() const {
    return std::max(1, static_cast<int>((_listTop - _listBottom) / kRowHeight));
}

void SidePanel::setMoves(const std::vector<std::string>& moves, bool whiteFirst) {
    if (moves == _moves && whiteFirst == _whiteFirst)
        return;
    // A new move, or a move taken back, means the player is looking at the
    // present: go back to following the end of the list.
    _moves      = moves;
    _whiteFirst = whiteFirst;
    _following  = true;
    rebuildMoves();
}

void SidePanel::scroll(int rows) {
    const int last = std::max(0, rowCount() - visibleRows());
    const int next = std::clamp(_firstRow - rows, 0, last);
    _following     = next == last;
    if (next != _firstRow) {
        _firstRow = next;
        rebuildMoves();
    }
}

void SidePanel::rebuildMoves() {
    _moveRows->removeAllChildren();
    if (_size.x <= 0.0f)
        return;

    const float width = _size.x - 2.0f * kPadding;

    if (_moves.empty()) {
        auto* empty = typeset::label(text::kNoMoves, 14.0f, theme::kFaint);
        empty->setAnchorPoint(Vec2(0.0f, 0.5f));
        empty->setPosition(Vec2(kPadding, _listTop - kRowHeight * 0.5f - 4.0f));
        _moveRows->addChild(empty);
        return;
    }

    const int rows    = rowCount();
    const int visible = visibleRows();
    const int last    = std::max(0, rows - visible);
    _firstRow         = _following ? last : std::clamp(_firstRow, 0, last);

    // Only the rows on screen are built. A long game has hundreds of moves,
    // and a label is a texture.
    const float numberWidth = 38.0f;
    const float column      = (width - numberWidth) * 0.5f;
    const int offset        = _whiteFirst ? 0 : 1;
    const int newest        = static_cast<int>(_moves.size()) - 1;

    auto* marks = DrawNode::create();
    _moveRows->addChild(marks);

    for (int row = _firstRow; row < std::min(rows, _firstRow + visible); ++row) {
        const float y = _listTop - 4.0f - (static_cast<float>(row - _firstRow) + 0.5f) * kRowHeight;

        auto* number = typeset::label(std::to_string(row + 1) + ".", 14.0f, theme::kFaint);
        number->setAnchorPoint(Vec2(1.0f, 0.5f));
        number->setPosition(Vec2(kPadding + numberWidth - 10.0f, y));
        _moveRows->addChild(number);

        for (int side = 0; side < 2; ++side) {
            const int index = row * 2 + side - offset;
            if (index < 0 || index > newest)
                continue;
            const float x = kPadding + numberWidth + static_cast<float>(side) * column;

            // The move just played is the one the eye is looking for.
            if (index == newest)
                shapes::roundedRect(marks, Vec2(x - 8.0f, y - kRowHeight * 0.5f + 2.0f),
                                    Vec2(column - 6.0f, kRowHeight - 4.0f), 6.0f, theme::kCardHover);

            auto* move = typeset::label(_moves[static_cast<std::size_t>(index)], 15.0f,
                                   index == newest ? theme::kInk : theme::kDim);
            move->setAnchorPoint(Vec2(0.0f, 0.5f));
            move->setPosition(Vec2(x, y));
            _moveRows->addChild(move);
        }
    }

    // A thin thumb on the right when there is more than fits.
    if (rows > visible) {
        const float track  = static_cast<float>(visible) * kRowHeight;
        const float length = std::max(18.0f, track * static_cast<float>(visible) / static_cast<float>(rows));
        const float travel = track - length;
        const float at     = last > 0 ? static_cast<float>(_firstRow) / static_cast<float>(last) : 0.0f;
        shapes::roundedRect(marks, Vec2(kPadding + width - 3.0f, _listTop - 4.0f - at * travel - length),
                            Vec2(3.0f, length), 1.5f, theme::kLine);
    }
}
