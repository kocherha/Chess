#include "LookDialog.h"

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

constexpr float kWidth   = 540.0f;
constexpr float kPadding = 30.0f;
constexpr float kRow     = 42.0f;
constexpr float kGap     = 10.0f;

// The preview: one of each piece, white along the bottom as on a board.
constexpr int       kColumns = 6, kRows = 2;
constexpr PieceType kOrder[kColumns] = {PieceType::King,   PieceType::Queen,  PieceType::Rook,
                                        PieceType::Bishop, PieceType::Knight, PieceType::Pawn};
constexpr float kLargestSquare = 72.0f, kSmallestSquare = 36.0f;

Label* makeLabel(const std::string& text, float points, const Color3B& colour) {
    Label* label = typeset::label(text, points, colour);
    label->setAnchorPoint(Vec2(0.0f, 0.5f));
    return label;
}

}  // namespace

LookDialog* LookDialog::create() {
    auto* node = new LookDialog();
    if (node->init()) {
        node->autorelease();
        return node;
    }
    delete node;
    return nullptr;
}

bool LookDialog::init() {
    if (!Node::init())
        return false;

    _back    = DrawNode::create();
    _preview = DrawNode::create();
    _pieces  = Node::create();
    addChild(_back);
    addChild(_preview);
    addChild(_pieces);

    _title = makeLabel(text::kLooks, 26.0f, theme::kInk);
    _title->enableBold();
    _piecesTitle = makeLabel(text::kLookPieces, 13.0f, theme::kFaint);
    _boardTitle  = makeLabel(text::kLookBoard, 13.0f, theme::kFaint);
    for (Label* label : {_title, _piecesTitle, _boardTitle})
        addChild(label);

    // One button for each set and each board there is: the tables in Look.cpp
    // decide how many, not this file.
    const auto choice = [this](const char* label, std::function<void()> click) {
        Button* button  = Button::create(label, Button::Style::Choice);
        button->onClick = std::move(click);
        addChild(button);
        _buttons.add(button);
        return button;
    };
    for (std::size_t i = 0; i < look::pieceSets().size(); ++i)
        _pieceButtons.push_back(
            choice(look::pieceSets()[i].name, [this, i] { choosePieces(static_cast<int>(i)); }));
    for (std::size_t i = 0; i < look::boards().size(); ++i)
        _boardButtons.push_back(choice(look::boards()[i].name, [this, i] { chooseBoard(static_cast<int>(i)); }));

    _done          = Button::create(text::kDone, Button::Style::Primary, 17.0f);
    _done->onClick = [this] {
        if (onDone)
            onDone();
    };
    addChild(_done);
    _buttons.add(_done);

    setVisible(false);
    return true;
}

void LookDialog::open() {
    setVisible(true);
    layout(_screen);
}

void LookDialog::choosePieces(int index) {
    if (index == look::pieceIndex())
        return;
    look::choosePieces(index);
    refresh();
    if (onChange)
        onChange();
}

void LookDialog::chooseBoard(int index) {
    if (index == look::boardIndex())
        return;
    look::chooseBoard(index);
    refresh();
    if (onChange)
        onChange();
}

void LookDialog::keyPressed(EventKeyboard::KeyCode code) {
    using Key = EventKeyboard::KeyCode;

    // Stepping wraps round: with a handful of choices, the end of the row is
    // one press from its beginning.
    const auto step = [](int from, int by, std::size_t count) {
        const int n = static_cast<int>(count);
        return ((from + by) % n + n) % n;
    };

    switch (code) {
    case Key::KEY_ENTER:
    case Key::KEY_KP_ENTER:
    case Key::KEY_SPACE:
    case Key::KEY_ESCAPE:
    case Key::KEY_V:
        if (onDone)
            onDone();
        break;
    case Key::KEY_LEFT_ARROW:
        chooseBoard(step(look::boardIndex(), -1, look::boards().size()));
        break;
    case Key::KEY_RIGHT_ARROW:
        chooseBoard(step(look::boardIndex(), 1, look::boards().size()));
        break;
    case Key::KEY_UP_ARROW:
        choosePieces(step(look::pieceIndex(), -1, look::pieceSets().size()));
        break;
    case Key::KEY_DOWN_ARROW:
        choosePieces(step(look::pieceIndex(), 1, look::pieceSets().size()));
        break;
    default:
        break;
    }
}

void LookDialog::refresh() {
    for (std::size_t i = 0; i < _pieceButtons.size(); ++i)
        _pieceButtons[i]->setChosen(static_cast<int>(i) == look::pieceIndex());
    for (std::size_t i = 0; i < _boardButtons.size(); ++i)
        _boardButtons[i]->setChosen(static_cast<int>(i) == look::boardIndex());

    _preview->clear();
    _pieces->removeAllChildren();
    if (_previewSquare <= 0.0f)
        return;

    const float s = _previewSquare;
    look::drawBoard(_preview, look::board(), _previewOrigin, s, kColumns, kRows, look::frameWidth(s));

    for (int row = 0; row < kRows; ++row) {
        const Color colour = row == 0 ? Color::White : Color::Black;
        for (int column = 0; column < kColumns; ++column) {
            if (Sprite* sprite = look::sprite(Piece{kOrder[column], colour}, s)) {
                sprite->setPosition(_previewOrigin + Vec2((static_cast<float>(column) + 0.5f) * s,
                                                          (static_cast<float>(row) + 0.5f) * s));
                _pieces->addChild(sprite);
            }
        }
    }
}

void LookDialog::layout(const Vec2& screen) {
    _screen = screen;
    if (!isVisible() || screen.x <= 0.0f)
        return;

    const float width = std::min(kWidth, screen.x - 40.0f);
    const float inner = width - 2.0f * kPadding;

    // Everything but the preview has a fixed height; the preview takes what
    // the window can spare, so the dialog still fits in the smallest one.
    const float above = kPadding + 34.0f + 24.0f;                             // title
    const float below = 26.0f + (22.0f + kRow + 22.0f) + (22.0f + kRow + 26.0f)  // the two rows of choices
                        + kRow + 4.0f + kPadding;                             // "done"
    const float spare = screen.y - 40.0f - above - below;
    float square      = std::min({kLargestSquare, std::floor(inner / (kColumns + 0.4f)),
                                  std::floor(spare / (kRows + 0.2f))});
    square            = std::max(kSmallestSquare, square);
    const float frame = look::frameWidth(square);
    const float previewHeight = square * kRows + 2.0f * frame;

    const float height = above + previewHeight + below;
    const Vec2 corner(std::floor((screen.x - width) * 0.5f), std::floor((screen.y - height) * 0.5f));

    _back->clear();
    shapes::rect(_back, Vec2::ZERO, screen, theme::kScrim);
    shapes::roundedRect(_back, corner, Vec2(width, height), 16.0f, theme::kPanel, 1.0f, theme::kLine);

    const float left = corner.x + kPadding;
    float y          = corner.y + height - kPadding;

    _title->setPosition(Vec2(left, y - 17.0f));
    y -= 34.0f + 24.0f;

    _previewSquare = square;
    _previewOrigin = Vec2(std::floor(corner.x + (width - square * kColumns) * 0.5f), y - frame - square * kRows);
    y -= previewHeight + 26.0f;

    const auto row = [&](Label* title, const std::vector<Button*>& buttons) {
        title->setPosition(Vec2(left, y - 8.0f));
        y -= 22.0f;
        const float count = static_cast<float>(buttons.size());
        const float cell  = (inner - (count - 1.0f) * kGap) / count;
        for (std::size_t i = 0; i < buttons.size(); ++i)
            buttons[i]->setFrame(Vec2(left + static_cast<float>(i) * (cell + kGap), y - kRow), Vec2(cell, kRow));
        y -= kRow;
    };
    row(_piecesTitle, _pieceButtons);
    y -= 22.0f;
    row(_boardTitle, _boardButtons);

    const float doneWidth = 150.0f;
    _done->setFrame(Vec2(corner.x + width - kPadding - doneWidth, corner.y + kPadding), Vec2(doneWidth, kRow + 4.0f));

    refresh();
}
