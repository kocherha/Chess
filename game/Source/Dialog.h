// What the game's dialogs have in common.
//
// A dialog is modal: while one is open the scene hands it every click and
// every key and gives the board none. Each lays itself out in the middle of
// the screen over a scrim, and owns the buttons on it.
#pragma once

#include "Button.h"

#include "axmol.h"

class Dialog : public ax::Node
{
public:
    bool isOpen() const { return isVisible(); }
    void close() { setVisible(false); }

    // Lays the dialog out in the middle of a screen of this size.
    virtual void layout(const ax::Vec2& screen) = 0;

    // True if the pointer moved onto or off a button, i.e. there is something
    // to redraw.
    bool mouseMove(const ax::Vec2& point) {
        _buttons.mouseMove(point);
        return _buttons.hoverChanged();
    }
    void mouseDown(const ax::Vec2& point) { _buttons.mouseDown(point); }
    void mouseUp(const ax::Vec2& point) { _buttons.mouseUp(point); }
    virtual void keyPressed(ax::EventKeyboard::KeyCode code) = 0;

protected:
    ButtonSet _buttons;
};
