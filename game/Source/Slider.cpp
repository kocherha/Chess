#include "Slider.h"

#include "Shapes.h"
#include "Theme.h"

#include <algorithm>
#include <cmath>

using namespace ax;

namespace {

constexpr float kTrack     = 4.0f;   // how thick the track is
constexpr float kStop      = 3.5f;   // the radius of a mark on it
constexpr float kThumb     = 8.0f;   // and of the thumb
constexpr float kThumbBig  = 9.5f;   // under the pointer, or held
constexpr float kOverhang  = 12.0f;  // how far past either end a press still counts

}  // namespace

Slider* Slider::create() {
    auto* slider = new Slider();
    if (slider->init()) {
        slider->autorelease();
        return slider;
    }
    delete slider;
    return nullptr;
}

bool Slider::init() {
    if (!Node::init())
        return false;
    _back = DrawNode::create();
    addChild(_back);
    return true;
}

void Slider::setFrame(const Vec2& origin, const Vec2& size) {
    setPosition(origin);
    _size = size;
    setContentSize(Size(size.x, size.y));
    redraw();
}

void Slider::setScale(int count, std::vector<int> stops) {
    _count = std::max(1, count);
    _stops = std::move(stops);
    _value = std::clamp(_value, 0, _count - 1);
    redraw();
}

void Slider::setValue(int value) {
    value = std::clamp(value, 0, _count - 1);
    if (value == _value)
        return;
    _value = value;
    redraw();
}

bool Slider::contains(const Vec2& worldPoint) const {
    for (const Node* node = this; node != nullptr; node = node->getParent()) {
        if (!node->isVisible())
            return false;
    }
    const Vec2 local = convertToNodeSpace(worldPoint);
    return local.x >= -kOverhang && local.x <= _size.x + kOverhang && local.y >= 0.0f && local.y <= _size.y;
}

// With stops, the track is cut into equal parts, one between each two
// neighbours, and a value lies in its part as far along as it lies between
// the two stops. Without them the whole track is one such part.
float Slider::offsetOf(int value) const {
    if (_count < 2)
        return 0.0f;
    if (_stops.size() < 2)
        return _size.x * static_cast<float>(value) / static_cast<float>(_count - 1);

    const float part = _size.x / static_cast<float>(_stops.size() - 1);
    std::size_t i    = 0;
    while (i + 2 < _stops.size() && _stops[i + 1] <= value)
        ++i;
    const float within = static_cast<float>(value - _stops[i]) / static_cast<float>(_stops[i + 1] - _stops[i]);
    return part * (static_cast<float>(i) + within);
}

int Slider::valueAt(float offset) const {
    if (_count < 2 || _size.x <= 0.0f)
        return 0;
    offset = std::clamp(offset, 0.0f, _size.x);
    if (_stops.size() < 2)
        return static_cast<int>(std::lround(offset / _size.x * static_cast<float>(_count - 1)));

    const float part = _size.x / static_cast<float>(_stops.size() - 1);
    const auto  i    = std::min(static_cast<std::size_t>(offset / part), _stops.size() - 2);
    const float within = offset / part - static_cast<float>(i);
    return _stops[i] + static_cast<int>(std::lround(within * static_cast<float>(_stops[i + 1] - _stops[i])));
}

void Slider::dragTo(const Vec2& worldPoint) {
    const int value = valueAt(convertToNodeSpace(worldPoint).x - _grab);
    if (value == _value)
        return;
    _value = value;
    redraw();
    if (onChange)
        onChange(_value);
}

bool Slider::mouseDown(const Vec2& worldPoint) {
    if (!contains(worldPoint))
        return false;
    _held = true;

    // On the thumb: it is picked up where it was touched. Anywhere else on
    // the strip: it comes to the pointer.
    const float x = convertToNodeSpace(worldPoint).x;
    _grab         = std::abs(x - offsetOf(_value)) <= kThumbBig ? x - offsetOf(_value) : 0.0f;
    dragTo(worldPoint);
    redraw();
    return true;
}

bool Slider::mouseMove(const Vec2& worldPoint) {
    if (_held) {
        // Wherever the pointer has got to — off the strip, off the dialog —
        // the thumb follows it for as long as the button is down.
        const int before = _value;
        dragTo(worldPoint);
        return _value != before;
    }
    const bool hovered = contains(worldPoint);
    if (hovered == _hovered)
        return false;
    _hovered = hovered;
    redraw();
    return true;
}

bool Slider::mouseUp(const Vec2& worldPoint) {
    if (!_held)
        return false;
    _held    = false;
    _hovered = contains(worldPoint);
    redraw();
    return true;
}

void Slider::redraw() {
    _back->clear();
    if (_size.x <= 0.0f)
        return;

    const float y  = std::floor(_size.y * 0.5f);
    const float at = offsetOf(_value);

    // The track, and over it the part already behind the thumb.
    const Color4F ahead = theme::kLine;
    const Color4F done  = theme::kAccent;
    shapes::roundedRect(_back, Vec2(-kTrack * 0.5f, y - kTrack * 0.5f), Vec2(_size.x + kTrack, kTrack),
                        kTrack * 0.5f, ahead);
    if (at > 0.0f)
        shapes::roundedRect(_back, Vec2(-kTrack * 0.5f, y - kTrack * 0.5f), Vec2(at + kTrack, kTrack),
                            kTrack * 0.5f, done);

    // The stops: beads on the track, lit up to where the thumb has got.
    for (const int stop : _stops)
        shapes::circle(_back, Vec2(offsetOf(stop), y), kStop, stop <= _value ? done : theme::kThumbHot);

    // The thumb. A ring of the panel's colour round it keeps it apart from
    // the track and the beads it passes over.
    const bool  hot    = _hovered || _held;
    const float radius = hot ? kThumbBig : kThumb;
    shapes::circle(_back, Vec2(at, y), radius + 2.5f, theme::kPanel);
    shapes::circle(_back, Vec2(at, y), radius, _held ? theme::kAccentHover : theme::rgb(236, 236, 240));
    if (_held)
        shapes::circle(_back, Vec2(at, y), radius - 3.5f, theme::rgb(255, 255, 255));
}
