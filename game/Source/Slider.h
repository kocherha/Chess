// A slider: a track, a thumb to drag along it, and marks at the places that
// have names.
//
// It holds a whole number from 0 to count - 1. The marks — "stops" — are
// spread evenly along the track whatever the values between them, so that a
// row of buttons above it, one per stop, has each button over its own mark;
// between two stops the values are spread evenly in turn. A stretch of the
// scale with more values in it is simply finer under the pointer.
//
// Like a Button it does not listen for the mouse itself: whoever owns it
// passes the events on, and is told through onChange when the value moves.
#pragma once

#include "axmol.h"

#include <functional>
#include <vector>

class Slider : public ax::Node
{
public:
    static Slider* create();

    // `origin` is where the thumb's centre is at the lowest value, at the
    // bottom of the strip; `size.x` is how far it travels and `size.y` how
    // tall the strip that answers to the pointer is. The thumb overhangs both
    // ends by half its width.
    void setFrame(const ax::Vec2& origin, const ax::Vec2& size);

    // The values run from 0 to count - 1. `stops` are the marked ones, in
    // rising order, the first of them 0 and the last count - 1.
    void setScale(int count, std::vector<int> stops);

    // Moves the thumb. Does not call onChange: that is for what the player did.
    void setValue(int value);
    int  value() const { return _value; }

    // The same contract as ButtonSet. mouseDown() takes hold of the thumb, or
    // brings it to a press anywhere on the strip, and returns true if the
    // press was the slider's. mouseMove() returns true if anything about the
    // slider changed and it needs to be seen. mouseUp() lets go, and returns
    // true if the slider was being held.
    bool mouseDown(const ax::Vec2& worldPoint);
    bool mouseMove(const ax::Vec2& worldPoint);
    bool mouseUp(const ax::Vec2& worldPoint);
    bool held() const { return _held; }

    std::function<void(int)> onChange;

private:
    bool  init() override;
    void  redraw();
    bool  contains(const ax::Vec2& worldPoint) const;
    float offsetOf(int value) const;   // along the track, from its start
    int   valueAt(float offset) const;
    void  dragTo(const ax::Vec2& worldPoint);

    int              _count = 1, _value = 0;
    std::vector<int> _stops;
    ax::Vec2         _size;
    bool             _hovered = false, _held = false;
    // Held by its edge, the thumb stays under the pointer where it was taken
    // rather than jumping to be centred on it.
    float            _grab = 0.0f;
    ax::DrawNode*    _back = nullptr;
};
