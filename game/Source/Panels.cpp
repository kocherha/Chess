#include "Panels.h"

#include "Look.h"
#include "Shapes.h"
#include "Strings.h"
#include "Text.h"
#include "Theme.h"

#include <algorithm>
#include <cmath>

using namespace ax;
using namespace chess;

namespace {

constexpr float kPadding   = 20.0f;
constexpr float kRowHeight = 26.0f;
constexpr float kButtonHeight = 40.0f;
constexpr float kGap       = 10.0f;

// The move list: a column for the move number, then white's move and black's.
constexpr float kNumberWidth = 38.0f;
// The strip down its right edge that belongs to the scroll bar. Wider than the
// thumb drawn in it, so that the thumb does not have to be hit exactly.
constexpr float kBarZone  = 14.0f;
constexpr float kBarWidth = 6.0f;

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

    auto* name = typeset::leftLabel(_name, 16.0f, _toMove ? theme::kInk : theme::kDim);
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
        auto* lead = typeset::leftLabel(_lead, 14.0f, theme::kDim);
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

    _title = typeset::leftLabel(text::kTitle, 26.0f, theme::kInk);
    typeset::embolden(_title);
    _subtitle   = typeset::leftLabel("", 14.0f, theme::kDim);
    _headline   = typeset::leftLabel("", 19.0f, theme::kInk);
    _detail     = typeset::leftLabel("", 14.0f, theme::kDim);
    _movesTitle = typeset::leftLabel(text::kMoves, 13.0f, theme::kFaint);
    for (Label* label : {_title, _subtitle, _headline, _detail, _movesTitle})
        addChild(label);
    _detail->setAnchorPoint(Vec2(0.0f, 1.0f));  // it may wrap, so it hangs from its top

    _highlights = DrawNode::create();
    addChild(_highlights);
    _moveRows = Node::create();
    addChild(_moveRows);

    modeGame    = Button::create(text::kModeGame, Button::Style::Choice, 14.0f);
    modePuzzles = Button::create(text::kModePuzzles, Button::Style::Choice, 14.0f);
    modeGame->setChosen(true);
    addChild(modeGame);
    addChild(modePuzzles);
    looks   = Button::create(text::kLooks, Button::Style::Plain, 14.0f);
    copy    = Button::create(text::kCopyAll, Button::Style::Plain, 12.5f);
    newGame = Button::create(text::kNewGame, Button::Style::Primary, 16.0f);
    undo    = Button::create(text::kUndo);
    hint    = Button::create(text::kHint);
    flip    = Button::create(text::kFlip);
    resign  = Button::create(text::kResign);
    for (Button* button : {looks, copy, newGame, undo, hint, flip, resign})
        addChild(button);
    copy->setEnabled(false);  // until there is something to copy

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
    y -= 42.0f;
    // Which of the two modes this is; the other is a click away.
    const float halfMode = std::floor((width - 8.0f) * 0.5f);
    modeGame->setFrame(Vec2(kPadding, y - 30.0f), Vec2(halfMode, 30.0f));
    modePuzzles->setFrame(Vec2(kPadding + width - halfMode, y - 30.0f), Vec2(halfMode, 30.0f));
    y -= 30.0f + 14.0f;
    _subtitle->setPosition(Vec2(kPadding, y - 8.0f));
    y -= 30.0f;

    // The status card: whose move it is, or how it ended. One line sits in
    // the middle of it; a second line goes underneath and may wrap, and the
    // card grows to hold it.
    _headline->setString(_headlineText);
    _headline->setColor(theme::toneInk(_tone));
    _detail->setString(typeset::wrap(_detailText, 14.0f, width - 32.0f));

    const bool twoLines    = !_detailText.empty();
    const float cardHeight = twoLines ? std::max(76.0f, 44.0f + _detail->getContentSize().height + 14.0f) : 76.0f;
    shapes::roundedRect(_back, Vec2(kPadding, y - cardHeight), Vec2(width, cardHeight), 10.0f, theme::kCard,
                        1.0f, theme::toneBorder(_tone));
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

    // The move list takes whatever is left between the two. Its heading
    // shares a line with the button that copies the game.
    _movesTitle->setPosition(Vec2(kPadding, y - 12.0f));
    copy->setFrame(Vec2(kPadding + width - 112.0f, y - 24.0f), Vec2(112.0f, 24.0f));
    y -= 32.0f;
    _listTop    = y;
    _listBottom = bottom;
    shapes::rect(_back, Vec2(kPadding, _listTop + 2.0f), Vec2(width, 1.0f), theme::kLine);

    _list.setVisibleRows(static_cast<int>((_listTop - _listBottom) / kRowHeight));
    rebuildMoves();
}

void SidePanel::setMoves(const std::vector<std::string>& moves, bool whiteFirst, int firstNumber) {
    if (!_list.setMoves(moves, whiteFirst, firstNumber))
        return;
    _draggingThumb = false;  // what it had hold of is not there any more
    copy->setEnabled(_list.moveCount() > 0);
    rebuildMoves();
}

void SidePanel::setMarks(const std::vector<Mark>& marks) {
    if (_list.setMarks(marks))
        rebuildMoves();
}

void SidePanel::setResult(const std::string& result) {
    if (_list.setResult(result))
        rebuildMoves();
}

bool SidePanel::scroll(float rows) {
    if (!_list.scroll(rows))
        return false;
    rebuildMoves();
    return true;
}

bool SidePanel::overList(const Vec2& worldPoint) const {
    const Vec2 local = convertToNodeSpace(worldPoint);
    return local.x >= 0.0f && local.x <= _size.x && local.y >= _listBottom && local.y <= _listTop + 2.0f;
}

// --- the move list under the mouse -----------------------------------------------

SidePanel::Thumb SidePanel::thumb() const {
    Thumb t;
    const int rows = _list.rowCount(), visible = _list.visibleRows();
    if (_size.x <= 0.0f || rows <= visible)
        return t;

    const float track = static_cast<float>(visible) * kRowHeight;
    t.shown  = true;
    t.top    = _listTop - 4.0f;
    t.length = std::max(24.0f, track * static_cast<float>(visible) / static_cast<float>(rows));
    t.travel = track - t.length;
    t.last   = _list.lastFirstRow();
    t.at     = t.top - t.travel * static_cast<float>(_list.firstRow()) / static_cast<float>(t.last);
    return t;
}

bool SidePanel::overBar(const Vec2& local) const {
    const Thumb t = thumb();
    return t.shown && local.x >= _size.x - kPadding - kBarZone && local.x <= _size.x - kPadding + 4.0f &&
           local.y <= t.top && local.y >= t.top - t.travel - t.length;
}

void SidePanel::dragThumbTo(float y) {
    const Thumb t = thumb();
    if (!t.shown || t.travel <= 0.0f)
        return;
    // Where the thumb's top edge would be, as a share of how far it can go.
    if (_list.scrollTo((t.top - (y + _thumbGrab)) / t.travel))
        rebuildMoves();
}

float SidePanel::columnWidth() const {
    return (_size.x - 2.0f * kPadding - kNumberWidth) * 0.5f;
}

float SidePanel::rowMiddle(int row) const {
    return _listTop - 4.0f - (static_cast<float>(row - _list.firstRow()) + 0.5f) * kRowHeight;
}

float SidePanel::cellLeft(int side) const {
    return kPadding + kNumberWidth - 8.0f + static_cast<float>(side) * columnWidth();
}

int SidePanel::moveAt(const Vec2& local) const {
    if (_list.moveCount() == 0 || local.y > _listTop - 4.0f)
        return -1;
    const int line = static_cast<int>((_listTop - 4.0f - local.y) / kRowHeight);
    if (line < 0 || line >= _list.visibleRows())
        return -1;

    const float column = columnWidth();
    const float x      = local.x - cellLeft(0);
    if (x < 0.0f || x >= 2.0f * column)
        return -1;
    const int side = x < column ? 0 : 1;
    // Each cell stops a little short of the next column.
    if (x - static_cast<float>(side) * column > column - 6.0f)
        return -1;

    return _list.moveIn(_list.firstRow() + line, side);
}

bool SidePanel::mouseDown(const Vec2& worldPoint) {
    const Vec2 local = convertToNodeSpace(worldPoint);

    if (overBar(local)) {
        const Thumb t  = thumb();
        _draggingThumb = true;
        if (local.y <= t.at && local.y >= t.at - t.length) {
            _thumbGrab = t.at - local.y;  // held where it was touched
        } else {
            _thumbGrab = t.length * 0.5f;  // a press on the track: the thumb comes to the pointer
            dragThumbTo(local.y);
        }
        redrawHighlights();  // the thumb is held
        return true;
    }

    const int index = moveAt(local);
    _list.press(index);
    return index >= 0;
}

bool SidePanel::mouseMove(const Vec2& worldPoint) {
    const Vec2 local = convertToNodeSpace(worldPoint);

    if (_draggingThumb) {
        const int before = _list.firstRow();
        dragThumbTo(local.y);
        return _list.firstRow() != before;
    }

    const bool hot  = overBar(local);
    const int hover = hot ? -1 : moveAt(local);
    if (hot == _barHot && hover == _list.hovered())
        return false;
    _barHot = hot;
    _list.setHovered(hover);
    redrawHighlights();
    return true;
}

void SidePanel::mouseUp(const Vec2& worldPoint) {
    const Vec2 local = convertToNodeSpace(worldPoint);

    if (_draggingThumb) {
        _draggingThumb = false;
        _barHot        = overBar(local);
        redrawHighlights();
        return;
    }

    if (const auto clicked = _list.release(moveAt(local)); clicked && onMoveClicked)
        onMoveClicked(*clicked);
}

void SidePanel::markMove(int index) {
    if (_list.setMarked(index))
        redrawHighlights();
}

void SidePanel::rebuildMoves() {
    _moveRows->removeAllChildren();
    _highlights->clear();
    if (_size.x <= 0.0f)
        return;

    if (_list.empty()) {
        auto* empty = typeset::leftLabel(text::kNoMoves, 14.0f, theme::kFaint);
        empty->setPosition(Vec2(kPadding, _listTop - kRowHeight * 0.5f - 4.0f));
        _moveRows->addChild(empty);
        return;
    }

    // Only the rows on screen are built. A long game has hundreds of moves,
    // and a label is a texture.
    const int first = _list.firstRow();
    const int end   = std::min(_list.rowCount(), first + _list.visibleRows());

    for (int row = first; row < end; ++row) {
        const float y = rowMiddle(row);

        if (_list.isResultRow(row)) {
            // Under the middle of the two columns of moves.
            auto* result = typeset::label(_list.result(), 15.0f, theme::kInk);
            typeset::embolden(result);
            result->setPosition(Vec2(cellLeft(1), y));
            _moveRows->addChild(result);
            continue;
        }

        auto* number = typeset::label(std::to_string(_list.number(row)) + ".", 14.0f, theme::kFaint);
        number->setAnchorPoint(Vec2(1.0f, 0.5f));
        number->setPosition(Vec2(textLeft(0) - 10.0f, y));
        _moveRows->addChild(number);

        for (int side = 0; side < 2; ++side) {
            const int index = _list.moveIn(row, side);
            if (index < 0)
                continue;
            const float x = textLeft(side);

            auto* move = typeset::leftLabel(_list.move(index), 15.0f, index == _list.newest() ? theme::kInk : theme::kDim);
            move->setPosition(Vec2(x, y));
            _moveRows->addChild(move);

            // Its mark, straight after it, in the mark's own colour.
            if (const Mark mark = _list.mark(index); mark != Mark::None) {
                const Color4F tint = theme::markColour(mark);
                auto* sign = typeset::leftLabel(suffix(mark), 15.0f,
                                            Color3B(static_cast<uint8_t>(tint.r * 255.0f),
                                                    static_cast<uint8_t>(tint.g * 255.0f),
                                                    static_cast<uint8_t>(tint.b * 255.0f)));
                typeset::embolden(sign);
                sign->setPosition(Vec2(x + move->getContentSize().width + 1.0f, y));
                _moveRows->addChild(sign);
            }
        }
    }

    redrawHighlights();
}

void SidePanel::redrawHighlights() {
    _highlights->clear();
    if (_size.x <= 0.0f || _list.empty())
        return;

    // The same rows and cells rebuildMoves() put the labels in.
    const int first = _list.firstRow();
    const int end   = std::min(_list.rowCount(), first + _list.visibleRows());
    const Vec2 cellSize(columnWidth() - 6.0f, kRowHeight - 4.0f);

    for (int row = first; row < end; ++row) {
        for (int side = 0; side < 2; ++side) {
            const int index = _list.moveIn(row, side);
            if (index < 0)
                continue;
            const Vec2 cellOrigin(cellLeft(side), rowMiddle(row) - kRowHeight * 0.5f + 2.0f);

            // The move just played is the one the eye is looking for; the one
            // under the pointer says it can be clicked; the one just copied
            // wears a ring until the notice goes away.
            if (index == _list.marked())
                shapes::roundedRect(_highlights, cellOrigin, cellSize, 6.0f, theme::kCardHover, 1.5f, theme::kAccent);
            else if (index == _list.newest())
                shapes::roundedRect(_highlights, cellOrigin, cellSize, 6.0f, theme::kCardHover);
            else if (index == _list.hovered())
                shapes::roundedRect(_highlights, cellOrigin, cellSize, 6.0f, theme::kCard);
        }
    }

    // The scroll bar, when there is more than fits: a faint track and a thumb
    // that lights up under the pointer and while it is held.
    if (const Thumb t = thumb(); t.shown) {
        const float x = _size.x - kPadding - kBarWidth;
        shapes::roundedRect(_highlights, Vec2(x, t.top - t.travel - t.length), Vec2(kBarWidth, t.travel + t.length),
                            kBarWidth * 0.5f, theme::kCard);
        shapes::roundedRect(_highlights, Vec2(x, t.at - t.length), Vec2(kBarWidth, t.length), kBarWidth * 0.5f,
                            _draggingThumb ? theme::kThumbHeld : _barHot ? theme::kThumbHot : theme::kLine);
    }
}
