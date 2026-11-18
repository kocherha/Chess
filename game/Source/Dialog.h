// What the game's dialogs have in common.
//
// A dialog is modal: while one is open the scene hands it every click and
// every key and gives the board none. Each lays itself out in the middle of
// the screen over a scrim, and owns the buttons on it.
#pragma once

#include "Button.h"
#include "Shapes.h"
#include "Theme.h"

#include "axmol.h"

#include <cmath>

class Dialog : public ax::Node
{
public:
    bool isOpen() const { return isVisible(); }
    void close() { setVisible(false); }

    // The screen is this size now: the dialog lays itself out in the middle
    // of it — at once if it is open, and otherwise when it next opens.
    void layout(const ax::Vec2& screen) {
        _screen = screen;
        relayout();
    }

    // True if the pointer moved onto or off a button, i.e. there is something
    // to redraw. Virtual for a dialog with more on it than buttons.
    virtual bool mouseMove(const ax::Vec2& point) {
        _buttons.mouseMove(point);
        return _buttons.hoverChanged();
    }
    virtual void mouseDown(const ax::Vec2& point) { _buttons.mouseDown(point); }
    virtual void mouseUp(const ax::Vec2& point) { _buttons.mouseUp(point); }
    virtual void keyPressed(ax::EventKeyboard::KeyCode code) = 0;

protected:
    // Lays the dialog out again on the screen it was last given: when it
    // opens, or when what is on it has changed size.
    void relayout() {
        if (isOpen() && _screen.x > 0.0f)
            place(_screen);
    }
    // The dialog's own layout, on a screen of this size. Only while it is open.
    virtual void place(const ax::Vec2& screen) = 0;

    // Clears `back` and draws on it what every dialog stands on: the scrim
    // over the whole screen, and the card of this size in its middle. Returns
    // the card's lower left corner, from which the rest is laid out.
    static ax::Vec2 drawCard(ax::DrawNode* back, const ax::Vec2& screen, const ax::Vec2& size) {
        const ax::Vec2 corner(std::floor((screen.x - size.x) * 0.5f), std::floor((screen.y - size.y) * 0.5f));
        back->clear();
        shapes::rect(back, ax::Vec2::ZERO, screen, theme::kScrim);
        shapes::roundedRect(back, corner, size, 16.0f, theme::kPanel, 1.0f, theme::kLine);
        return corner;
    }

    ButtonSet _buttons;

private:
    ax::Vec2 _screen;
};
