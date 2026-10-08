#include "Button.h"

#include "Shapes.h"
#include "Theme.h"

#include <algorithm>

using namespace ax;

Button* Button::create(const std::string& text, Style style, float fontSize) {
    auto* button = new Button();
    if (button->initWith(text, style, fontSize)) {
        button->autorelease();
        return button;
    }
    delete button;
    return nullptr;
}

bool Button::initWith(const std::string& text, Style style, float fontSize) {
    if (!Node::init())
        return false;
    _style = style;

    _back = DrawNode::create();
    addChild(_back);

    _label = Label::createWithSystemFont(text, theme::kFont, fontSize);
    addChild(_label);

    setFrame(Vec2::ZERO, Vec2(120.0f, 36.0f));
    return true;
}

void Button::setFrame(const Vec2& origin, const Vec2& size) {
    setPosition(origin);
    _size = size;
    setContentSize(Size(size.x, size.y));
    _label->setPosition(Vec2(size.x * 0.5f, size.y * 0.5f));
    redraw();
}

void Button::setText(const std::string& text) {
    _label->setString(text);
}

void Button::setEnabled(bool enabled) {
    if (_enabled == enabled)
        return;
    _enabled = enabled;
    if (!enabled)
        _hovered = _pressed = false;
    redraw();
}

void Button::setChosen(bool chosen) {
    if (_chosen == chosen)
        return;
    _chosen = chosen;
    redraw();
}

void Button::setHovered(bool hovered) {
    hovered = hovered && _enabled;
    if (_hovered == hovered)
        return;
    _hovered = hovered;
    redraw();
}

void Button::setPressed(bool pressed) {
    pressed = pressed && _enabled;
    if (_pressed == pressed)
        return;
    _pressed = pressed;
    redraw();
}

bool Button::contains(const Vec2& worldPoint) const {
    for (const Node* node = this; node != nullptr; node = node->getParent()) {
        if (!node->isVisible())
            return false;
    }
    const Vec2 local = convertToNodeSpace(worldPoint);
    return local.x >= 0.0f && local.y >= 0.0f && local.x <= _size.x && local.y <= _size.y;
}

void Button::redraw() {
    _back->clear();

    const bool lit = _style == Style::Primary || (_style == Style::Choice && _chosen);

    Color4F fill;
    if (lit)
        fill = _pressed ? theme::kAccentPress : _hovered ? theme::kAccentHover : theme::kAccent;
    else
        fill = _pressed ? theme::kCardPress : _hovered ? theme::kCardHover : theme::kCard;

    const float radius = std::min(8.0f, _size.y * 0.5f);
    if (lit)
        shapes::roundedRect(_back, Vec2::ZERO, _size, radius, fill);
    else
        shapes::roundedRect(_back, Vec2::ZERO, _size, radius, fill, 1.0f, theme::kLine);

    _label->setColor(lit ? theme::kOnAccent : theme::kInk);
    // A disabled button stays where it is and goes quiet, rather than
    // vanishing and making the layout jump.
    _back->setOpacity(_enabled ? 255 : 90);
    _label->setOpacity(_enabled ? 255 : 90);
}

// --- ButtonSet ---------------------------------------------------------------------

void ButtonSet::clear() {
    _buttons.clear();
    _hovered = _pressed = nullptr;
}

Button* ButtonSet::at(const Vec2& point) const {
    // Later buttons are drawn on top, so they are asked first.
    for (auto it = _buttons.rbegin(); it != _buttons.rend(); ++it) {
        if ((*it)->enabled() && (*it)->contains(point))
            return *it;
    }
    return nullptr;
}

bool ButtonSet::mouseMove(const Vec2& point) {
    Button* over = at(point);
    // A held button going up or down is covered too: it only does so when the
    // pointer crosses its edge, which is a change of `over`.
    _hoverChanged = over != _hovered;
    if (over != _hovered) {
        if (_hovered != nullptr)
            _hovered->setHovered(false);
        _hovered = over;
        if (_hovered != nullptr)
            _hovered->setHovered(true);
    }
    // Sliding off a held button lets it up; sliding back presses it again.
    if (_pressed != nullptr)
        _pressed->setPressed(over == _pressed);
    return over != nullptr || _pressed != nullptr;
}

bool ButtonSet::mouseDown(const Vec2& point) {
    mouseMove(point);
    _pressed = at(point);
    if (_pressed != nullptr)
        _pressed->setPressed(true);
    return _pressed != nullptr;
}

bool ButtonSet::mouseUp(const Vec2& point) {
    Button* pressed = _pressed;
    _pressed        = nullptr;
    if (pressed == nullptr)
        return false;

    pressed->setPressed(false);
    // A click is a press and a release on the same button. The callback may
    // rebuild the very screen this button is on, so nothing touches `pressed`
    // after it.
    if (pressed->enabled() && pressed->contains(point) && pressed->onClick) {
        auto action = pressed->onClick;
        action();
    }
    return true;
}
