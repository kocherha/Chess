// A button: a rounded rectangle with a line of text, and the handful of states
// a pointer can put it in.
//
// The scene owns the mouse. It asks each button "is this point yours?" and
// tells it when it is hovered or pressed, rather than every button listening
// for events itself — one listener, one place where a click is decided, and no
// question of which of two overlapping things gets it.
#pragma once

#include "axmol.h"

#include <functional>
#include <string>
#include <vector>

class Button : public ax::Node
{
public:
    enum class Style {
        Primary,  // the one thing this screen is for
        Plain,    // everything else
        Choice,   // one of several; the chosen one is lit
    };

    static Button* create(const std::string& text, Style style = Style::Plain, float fontSize = 15.0f);

    // Laid out from the bottom-left corner, like everything else here.
    void setFrame(const ax::Vec2& origin, const ax::Vec2& size);

    void setText(const std::string& text);
    void setEnabled(bool enabled);
    bool enabled() const { return _enabled; }
    void setChosen(bool chosen);
    bool chosen() const { return _chosen; }

    void setHovered(bool hovered);
    void setPressed(bool pressed);

    // Is this point (in world coordinates) on the button? False while the
    // button or anything it sits in is hidden.
    bool contains(const ax::Vec2& worldPoint) const;

    std::function<void()> onClick;

private:
    bool initWith(const std::string& text, Style style, float fontSize);
    void redraw();

    Style _style = Style::Plain;
    bool _enabled = true, _chosen = false, _hovered = false, _pressed = false;
    ax::Vec2 _size;
    ax::DrawNode* _back  = nullptr;
    ax::Label*    _label = nullptr;
};

// The buttons that are live on one layer of the screen, and which of them the
// pointer is doing something to.
class ButtonSet
{
public:
    void add(Button* button) { _buttons.push_back(button); }
    void clear();

    // Each returns true if a button was involved, so the caller knows whether
    // the event is spoken for.
    bool mouseMove(const ax::Vec2& point);
    bool mouseDown(const ax::Vec2& point);
    bool mouseUp(const ax::Vec2& point);

    // Did the last mouseMove() change how any button looks? Most do not — the
    // pointer is somewhere over the board — and then there is nothing to draw.
    bool hoverChanged() const { return _hoverChanged; }

private:
    Button* at(const ax::Vec2& point) const;

    std::vector<Button*> _buttons;
    Button* _hovered = nullptr;
    Button* _pressed = nullptr;
    bool    _hoverChanged = false;
};
