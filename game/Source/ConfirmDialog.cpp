#include "ConfirmDialog.h"

#include "Shapes.h"
#include "Strings.h"
#include "Text.h"
#include "Theme.h"

#include <algorithm>
#include <cmath>

using namespace ax;

namespace {

constexpr float kWidth   = 470.0f;
constexpr float kPadding = 30.0f;
constexpr float kRow     = 46.0f;
constexpr float kGap     = 10.0f;

}  // namespace

ConfirmDialog* ConfirmDialog::create() {
    auto* node = new ConfirmDialog();
    if (node->init()) {
        node->autorelease();
        return node;
    }
    delete node;
    return nullptr;
}

bool ConfirmDialog::init() {
    if (!Node::init())
        return false;

    _back = DrawNode::create();
    addChild(_back);

    _title = typeset::leftLabel("", 22.0f, theme::kInk);
    typeset::embolden(_title);
    _message = typeset::label("", 15.0f, theme::kDim);
    _message->setAnchorPoint(Vec2(0.0f, 1.0f));
    addChild(_title);
    addChild(_message);

    _go            = Button::create("", Button::Style::Plain, 16.0f);
    _keep          = Button::create(text::kKeepPlaying, Button::Style::Primary, 16.0f);
    _go->onClick   = [this] { proceed(); };
    _keep->onClick = [this] { stay(); };
    for (Button* button : {_go, _keep}) {
        addChild(button);
        _buttons.add(button);
    }

    setVisible(false);
    return true;
}

void ConfirmDialog::open(const std::string& title, const std::string& message, const std::string& proceed,
                         std::function<void()> onProceed, std::function<void()> onStay) {
    _quitting = false;
    if (isOpen()) {
        const auto then = std::move(_onStay);
        _onProceed = _onStay = nullptr;
        if (then)
            then();
    }

    _title->setString(title);
    _text = message;
    _go->setText(proceed);
    _onProceed = std::move(onProceed);
    _onStay    = std::move(onStay);

    setVisible(true);
    relayout();
}

// The callback is taken out before it is called: it may well open this very
// dialog again with a different question.
void ConfirmDialog::askBeforeQuitting(bool& closing) {
    if (_quitting && isOpen())
        return;
    closing = false;
    open(text::kUnfinishedTitle, text::kLoseByQuit, text::kQuit, [] { ax::Director::getInstance()->end(); });
    _quitting = true;
}

void ConfirmDialog::proceed() {
    const auto then = std::move(_onProceed);
    _onProceed = _onStay = nullptr;
    close();
    if (then)
        then();
}

void ConfirmDialog::stay() {
    const auto then = std::move(_onStay);
    _onProceed = _onStay = nullptr;
    close();
    if (then)
        then();
}

void ConfirmDialog::keyPressed(EventKeyboard::KeyCode code) {
    using Key = EventKeyboard::KeyCode;
    switch (code) {
    case Key::KEY_ENTER:
    case Key::KEY_KP_ENTER:
    case Key::KEY_SPACE:
    case Key::KEY_ESCAPE:
        stay();
        break;
    default:
        break;
    }
}

void ConfirmDialog::place(const Vec2& screen) {
    const float width = std::min(kWidth, screen.x - 40.0f);
    const float inner = width - 2.0f * kPadding;

    _message->setString(typeset::wrap(_text, 15.0f, inner));
    const float messageHeight = _message->getContentSize().height;

    const float height = kPadding + 30.0f + 18.0f + messageHeight + 28.0f + kRow + kPadding;
    const Vec2 corner = drawCard(_back, screen, Vec2(width, height));

    const float left = corner.x + kPadding;
    float y          = corner.y + height - kPadding;
    _title->setPosition(Vec2(left, y - 15.0f));
    y -= 30.0f + 18.0f;
    _message->setPosition(Vec2(left, y));

    // Side by side, the one that stays on the right where the eye ends up.
    const float keepWidth = std::floor((inner - kGap) * 0.5f);
    _keep->setFrame(Vec2(corner.x + width - kPadding - keepWidth, corner.y + kPadding), Vec2(keepWidth, kRow));
    _go->setFrame(Vec2(left, corner.y + kPadding), Vec2(inner - keepWidth - kGap, kRow));
}
