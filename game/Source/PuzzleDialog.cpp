#include "PuzzleDialog.h"

#include "Shapes.h"
#include "Strings.h"
#include "Text.h"
#include "Theme.h"

#include <algorithm>
#include <cmath>

using namespace ax;

namespace {

constexpr float kWidth   = 760.0f;
constexpr float kPadding = 28.0f;
constexpr float kRow     = 34.0f;
constexpr float kGap     = 8.0f;
constexpr int   kAcross  = 3;
// In a short window: four to a line, and lower.
constexpr float kShortScreen = 700.0f;
constexpr float kRowShort    = 28.0f;
constexpr float kGapShort    = 6.0f;

}  // namespace

PuzzleDialog* PuzzleDialog::create() {
    auto* node = new PuzzleDialog();
    if (node->init()) {
        node->autorelease();
        return node;
    }
    delete node;
    return nullptr;
}

bool PuzzleDialog::init() {
    if (!Node::init())
        return false;

    _back    = DrawNode::create();
    _content = Node::create();
    _ticks   = DrawNode::create();
    addChild(_back);
    addChild(_content);
    addChild(_ticks);

    _title = typeset::label(text::kCollection, 24.0f, theme::kInk);
    typeset::embolden(_title);
    _title->setAnchorPoint(Vec2(0.0f, 0.5f));
    addChild(_title);

    _close          = Button::create(text::kCancel, Button::Style::Plain, 15.0f);
    _close->onClick = [this] {
        if (onCancel)
            onCancel();
    };
    addChild(_close);

    setVisible(false);
    return true;
}

void PuzzleDialog::open() {
    rebuild();
    setVisible(true);
    layout(_screen);
}

void PuzzleDialog::rebuild() {
    _content->removeAllChildren();
    _groups.clear();
    _buttons.clear();
    _buttons.add(_close);
    _empty = nullptr;

    // The collection by the length of its mates, then the player's own.
    const auto group = [this](const std::string& title) -> Group& {
        // Returned for the one line that fills it at once; the vector grows,
        // so nothing holds on to it.
        Group g;
        g.title = typeset::label(title, 13.0f, theme::kFaint);
        g.title->setAnchorPoint(Vec2(0.0f, 0.5f));
        _content->addChild(g.title);
        _groups.push_back(std::move(g));
        return _groups.back();
    };
    group("Мат в 1 хід");
    group("Мат у 2 ходи");
    group("Мат у 3–5 ходів");
    for (const Puzzle& puzzle : puzzles::collection()) {
        const std::size_t index = puzzle.mateIn <= 1 ? 0 : puzzle.mateIn == 2 ? 1 : 2;
        _groups[index].puzzles.push_back(puzzle);
    }
    group(text::kMine).puzzles = puzzles::mine();

    for (Group& g : _groups) {
        for (const Puzzle& puzzle : g.puzzles) {
            std::string label = puzzle.title;
            if (puzzle.mine)
                label += puzzle.mateIn > 0 ? " · #" + std::to_string(puzzle.mateIn) : std::string(" · хід");
            Button* button  = Button::create(label, Button::Style::Plain, 12.5f);
            button->onClick = [this, puzzle] {
                if (onChoose)
                    onChoose(puzzle);
            };
            _content->addChild(button);
            _buttons.add(button);
            g.buttons.push_back(button);
        }
    }
    if (_groups.back().puzzles.empty()) {
        _empty = typeset::label(text::kMineEmpty, 13.0f, theme::kDim);
        _empty->setAnchorPoint(Vec2(0.0f, 1.0f));
        _content->addChild(_empty);
    }
}

void PuzzleDialog::keyPressed(EventKeyboard::KeyCode code) {
    if (code == EventKeyboard::KeyCode::KEY_ESCAPE && onCancel)
        onCancel();
}

void PuzzleDialog::layout(const Vec2& screen) {
    _screen = screen;
    if (!isVisible() || screen.x <= 0.0f)
        return;

    const bool  shortScreen = screen.y < kShortScreen;
    const std::size_t across = shortScreen ? kAcross + 1 : kAcross;
    const float row    = shortScreen ? kRowShort : kRow;
    const float gap    = shortScreen ? kGapShort : kGap;
    const float head   = shortScreen ? 22.0f : 26.0f;  // a group's title

    const float width = std::min(kWidth, screen.x - 40.0f);
    const float inner = width - 2.0f * kPadding;
    const float cell  = std::floor((inner - (across - 1) * gap) / across);

    // The player's own may be many; the window decides how many rows of them
    // there is room for, and those shown are the latest.
    const auto rows = [across](std::size_t n) { return static_cast<float>((n + across - 1) / across); };
    float fixed = kPadding + 40.0f + 10.0f + kPadding + row + 16.0f;
    for (std::size_t i = 0; i + 1 < _groups.size(); ++i)
        fixed += head + rows(_groups[i].puzzles.size()) * (row + gap) + 8.0f;
    fixed += head;
    const float spare  = std::max(row, screen.y - 40.0f - fixed);
    Group& mine        = _groups.back();
    const std::size_t fitRows = static_cast<std::size_t>(std::max(1.0f, std::floor((spare + gap) / (row + gap))));
    const std::size_t shownMine = std::min(mine.buttons.size(), fitRows * across);
    for (std::size_t i = 0; i < mine.buttons.size(); ++i)
        mine.buttons[i]->setVisible(i >= mine.buttons.size() - shownMine);
    const float mineHeight = mine.buttons.empty() ? 40.0f : rows(shownMine) * (row + gap);

    const float height = fixed + mineHeight;
    const Vec2 corner(std::floor((screen.x - width) * 0.5f), std::floor((screen.y - height) * 0.5f));

    _back->clear();
    _ticks->clear();
    shapes::rect(_back, Vec2::ZERO, screen, theme::kScrim);
    shapes::roundedRect(_back, corner, Vec2(width, height), 16.0f, theme::kPanel, 1.0f, theme::kLine);

    const float left = corner.x + kPadding;
    float y          = corner.y + height - kPadding;
    _title->setPosition(Vec2(left, y - 16.0f));
    y -= 40.0f + 10.0f;

    for (Group& g : _groups) {
        g.title->setPosition(Vec2(left, y - 9.0f));
        y -= head;
        std::size_t slot = 0;
        for (Button* button : g.buttons) {
            if (!button->isVisible())
                continue;
            const float column = static_cast<float>(slot % across), line = static_cast<float>(slot / across);
            const Vec2 at(left + column * (cell + gap), y - row - line * (row + gap));
            button->setFrame(at, Vec2(cell, row));
            ++slot;
        }
        if (&g == &_groups.back() && _empty != nullptr) {
            _empty->setString(typeset::wrap(text::kMineEmpty, 13.0f, inner));
            _empty->setPosition(Vec2(left, y - 4.0f));
        }
        y -= (slot == 0 ? 40.0f : rows(slot) * (row + gap)) + 8.0f;
    }

    // Solved ones: a tick in the right end of the button.
    for (Group& g : _groups) {
        for (std::size_t i = 0; i < g.buttons.size(); ++i) {
            Button* button = g.buttons[i];
            if (!button->isVisible() || !puzzles::solved(g.puzzles[i].id))
                continue;
            const Vec2 c = button->getPosition() + Vec2(cell - 13.0f, row * 0.5f);
            shapes::convex(_ticks, {c + Vec2(-6.0f, 0.5f), c + Vec2(-4.0f, 2.5f), c + Vec2(-1.0f, -0.5f), c + Vec2(-2.5f, -3.5f)},
                           theme::kAccent);
            shapes::convex(_ticks, {c + Vec2(-2.5f, -3.5f), c + Vec2(-1.0f, -0.5f), c + Vec2(6.0f, 6.5f), c + Vec2(7.5f, 4.5f)},
                           theme::kAccent);
        }
    }

    const float closeWidth = 150.0f;
    _close->setFrame(Vec2(corner.x + width - kPadding - closeWidth, corner.y + kPadding), Vec2(closeWidth, row + 4.0f));
}
