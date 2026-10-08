#include "NewGameDialog.h"

#include "Shapes.h"
#include "Strings.h"
#include "Text.h"
#include "Theme.h"

#include <algorithm>
#include <cmath>
#include <initializer_list>

using namespace ax;

namespace {

constexpr float kWidth   = 540.0f;
constexpr float kPadding = 30.0f;
constexpr float kRow     = 42.0f;
constexpr float kGap     = 10.0f;

Label* makeLabel(const std::string& text, float points, const Color3B& colour) {
    Label* label = typeset::label(text, points, colour);
    label->setAnchorPoint(Vec2(0.0f, 0.5f));
    return label;
}

}  // namespace

NewGameDialog* NewGameDialog::create() {
    auto* node = new NewGameDialog();
    if (node->init()) {
        node->autorelease();
        return node;
    }
    delete node;
    return nullptr;
}

bool NewGameDialog::init() {
    if (!Node::init())
        return false;

    _back = DrawNode::create();
    addChild(_back);

    _title = makeLabel(text::kNewGame, 26.0f, theme::kInk);
    _title->enableBold();
    _opponentTitle    = makeLabel(text::kOpponent, 13.0f, theme::kFaint);
    _sideTitle        = makeLabel(text::kYourColour, 13.0f, theme::kFaint);
    _levelTitle       = makeLabel(text::kDifficulty, 13.0f, theme::kFaint);
    _levelName        = makeLabel("", 17.0f, theme::kInk);
    _levelDescription = makeLabel("", 14.0f, theme::kDim);
    _levelDescription->setAnchorPoint(Vec2(0.0f, 1.0f));
    _notice = makeLabel(text::kNoEngine, 14.0f, theme::kWarn);
    _notice->setAnchorPoint(Vec2(0.0f, 1.0f));
    for (Label* label : {_title, _opponentTitle, _sideTitle, _levelTitle, _levelName, _levelDescription, _notice})
        addChild(label);

    const auto choice = [this](const char* label) {
        Button* button = Button::create(label, Button::Style::Choice);
        addChild(button);
        _buttons.add(button);
        return button;
    };

    _computer = choice(text::kVsComputer);
    _human    = choice(text::kVsHuman);
    _computer->onClick = [this] { choose(GameSetup::Opponent::Computer); };
    _human->onClick    = [this] { choose(GameSetup::Opponent::Human); };

    _white  = choice(text::kWhite);
    _random = choice(text::kRandom);
    _black  = choice(text::kBlack);
    _white->onClick  = [this] { choose(GameSetup::Side::White); };
    _random->onClick = [this] { choose(GameSetup::Side::Random); };
    _black->onClick  = [this] { choose(GameSetup::Side::Black); };

    for (std::size_t i = 0; i < _levels.size(); ++i) {
        const int number = static_cast<int>(i) + 1;
        _levels[i]       = Button::create(std::to_string(number), Button::Style::Choice, 17.0f);
        _levels[i]->onClick = [this, number] { chooseLevel(number); };
        addChild(_levels[i]);
        _buttons.add(_levels[i]);
    }

    _cancel = Button::create(text::kCancel);
    _play   = Button::create(text::kPlay, Button::Style::Primary, 17.0f);
    _cancel->onClick = [this] {
        if (onCancel)
            onCancel();
    };
    _play->onClick = [this] {
        if (onPlay)
            onPlay(_setup);
    };
    addChild(_cancel);
    addChild(_play);
    _buttons.add(_cancel);
    _buttons.add(_play);

    setVisible(false);
    return true;
}

void NewGameDialog::open(const GameSetup& setup, bool canCancel, bool engineAvailable, int eloMin, int eloMax) {
    _setup           = setup;
    _setup.level     = chess::levelByNumber(setup.level).number;
    _canCancel       = canCancel;
    _engineAvailable = engineAvailable;
    _eloMin          = eloMin;
    _eloMax          = eloMax;
    if (!engineAvailable)
        _setup.opponent = GameSetup::Opponent::Human;

    setVisible(true);
    layout(_screen);
}

void NewGameDialog::choose(GameSetup::Opponent opponent) {
    _setup.opponent = opponent;
    layout(_screen);
}

void NewGameDialog::choose(GameSetup::Side side) {
    _setup.side = side;
    refresh();
}

void NewGameDialog::chooseLevel(int number) {
    _setup.level = chess::levelByNumber(number).number;
    // The description can be one line or two, and the card is sized to it.
    layout(_screen);
}

void NewGameDialog::keyPressed(EventKeyboard::KeyCode code) {
    using Key = EventKeyboard::KeyCode;
    const bool computer = _setup.opponent == GameSetup::Opponent::Computer;

    switch (code) {
    case Key::KEY_ENTER:
    case Key::KEY_KP_ENTER:
    case Key::KEY_SPACE:
        if (onPlay)
            onPlay(_setup);
        break;
    case Key::KEY_ESCAPE:
        if (_canCancel && onCancel)
            onCancel();
        break;
    case Key::KEY_LEFT_ARROW:
    case Key::KEY_DOWN_ARROW:
        if (computer)
            chooseLevel(_setup.level - 1);
        break;
    case Key::KEY_RIGHT_ARROW:
    case Key::KEY_UP_ARROW:
        if (computer)
            chooseLevel(_setup.level + 1);
        break;
    default:
        // The digits pick a level directly.
        if (computer && code >= Key::KEY_1 && code <= Key::KEY_8)
            chooseLevel(static_cast<int>(code) - static_cast<int>(Key::KEY_1) + 1);
        break;
    }
}

void NewGameDialog::refresh() {
    const bool computer = _setup.opponent == GameSetup::Opponent::Computer;

    _computer->setChosen(computer);
    _human->setChosen(!computer);
    _computer->setEnabled(_engineAvailable);

    _white->setChosen(_setup.side == GameSetup::Side::White);
    _random->setChosen(_setup.side == GameSetup::Side::Random);
    _black->setChosen(_setup.side == GameSetup::Side::Black);

    for (std::size_t i = 0; i < _levels.size(); ++i)
        _levels[i]->setChosen(static_cast<int>(i) + 1 == _setup.level);

    const chess::Level& level = chess::levelByNumber(_setup.level);
    int elo                   = level.elo;
    if (_eloMin > 0 && _eloMax >= _eloMin)
        elo = std::clamp(elo, _eloMin, _eloMax);
    _levelName->setString(text::levelName(level.number));
    _levelDescription->setString(typeset::wrap(text::levelDescription(level, elo), 14.0f, _textWidth));
    _notice->setString(typeset::wrap(text::kNoEngine, 14.0f, _textWidth));
}

void NewGameDialog::layout(const Vec2& screen) {
    _screen = screen;
    if (!isVisible() || screen.x <= 0.0f)
        return;

    const bool computer = _setup.opponent == GameSetup::Opponent::Computer;
    const float width   = std::min(kWidth, screen.x - 40.0f);
    const float inner   = width - 2.0f * kPadding;

    // Text first: the description wraps, and the card is as tall as it needs.
    _textWidth = inner;
    refresh();

    // Only what applies is shown. Two people at one board have no "your
    // colour" and no difficulty.
    for (Node* node : std::initializer_list<Node*>{_sideTitle, _white, _random, _black, _levelTitle, _levelName,
                                                   _levelDescription})
        node->setVisible(computer);
    for (Button* button : _levels)
        button->setVisible(computer);
    _notice->setVisible(!_engineAvailable);
    _cancel->setVisible(_canCancel);

    // Room for two lines whatever the level, so the card does not change
    // height as the player steps through them.
    const float descriptionHeight = std::max(36.0f, _levelDescription->getContentSize().height);
    const float noticeHeight      = _notice->getContentSize().height;

    float height = kPadding + 34.0f + 22.0f;        // title
    height += 22.0f + kRow + 22.0f;                 // opponent
    if (computer) {
        height += 22.0f + kRow + 22.0f;             // colour
        height += 22.0f + kRow + 14.0f + 24.0f + descriptionHeight + 18.0f;  // level
    }
    if (!_engineAvailable)
        height += noticeHeight + 20.0f;
    height += kRow + 4.0f + kPadding;               // buttons

    const Vec2 corner(std::floor((screen.x - width) * 0.5f), std::floor((screen.y - height) * 0.5f));

    _back->clear();
    shapes::rect(_back, Vec2::ZERO, screen, theme::kScrim);
    shapes::roundedRect(_back, corner, Vec2(width, height), 16.0f, theme::kPanel, 1.0f, theme::kLine);

    const float left = corner.x + kPadding;
    float y          = corner.y + height - kPadding;

    _title->setPosition(Vec2(left, y - 17.0f));
    y -= 34.0f + 22.0f;

    _opponentTitle->setPosition(Vec2(left, y - 8.0f));
    y -= 22.0f;
    {
        // The second label is the longer one, so it gets the larger share.
        const float first = std::floor((inner - kGap) * 0.4f);
        _computer->setFrame(Vec2(left, y - kRow), Vec2(first, kRow));
        _human->setFrame(Vec2(left + first + kGap, y - kRow), Vec2(inner - first - kGap, kRow));
    }
    y -= kRow + 22.0f;

    if (!_engineAvailable) {
        _notice->setPosition(Vec2(left, y));
        y -= noticeHeight + 20.0f;
    }

    if (computer) {
        _sideTitle->setPosition(Vec2(left, y - 8.0f));
        y -= 22.0f;
        const float third = (inner - 2.0f * kGap) / 3.0f;
        _white->setFrame(Vec2(left, y - kRow), Vec2(third, kRow));
        _random->setFrame(Vec2(left + third + kGap, y - kRow), Vec2(third, kRow));
        _black->setFrame(Vec2(left + 2.0f * (third + kGap), y - kRow), Vec2(third, kRow));
        y -= kRow + 22.0f;

        _levelTitle->setPosition(Vec2(left, y - 8.0f));
        y -= 22.0f;
        const float cell = (inner - 7.0f * kGap) / 8.0f;
        for (std::size_t i = 0; i < _levels.size(); ++i)
            _levels[i]->setFrame(Vec2(left + static_cast<float>(i) * (cell + kGap), y - kRow), Vec2(cell, kRow));
        y -= kRow + 14.0f;

        _levelName->setPosition(Vec2(left, y - 12.0f));
        y -= 24.0f;
        _levelDescription->setPosition(Vec2(left, y - 2.0f));
        y -= descriptionHeight + 18.0f;
    }

    const float bottom = corner.y + kPadding;
    const float playWidth = 150.0f, cancelWidth = 130.0f;
    _play->setFrame(Vec2(corner.x + width - kPadding - playWidth, bottom), Vec2(playWidth, kRow + 4.0f));
    _cancel->setFrame(Vec2(corner.x + width - kPadding - playWidth - kGap - cancelWidth, bottom),
                      Vec2(cancelWidth, kRow + 4.0f));
}
