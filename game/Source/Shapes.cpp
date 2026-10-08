#include "Shapes.h"

#include <algorithm>
#include <cmath>
#include <vector>

using namespace ax;

namespace shapes {

namespace {

constexpr float kPi = 3.14159265358979f;

Color4F clear(const Color4F& colour) {
    return Color4F(colour.r, colour.g, colour.b, 0.0f);
}

int segmentsFor(float radius) {
    // Enough that the straight pieces are shorter than a couple of pixels.
    const float circumference = 2.0f * kPi * radius / pixel();
    return std::clamp(static_cast<int>(circumference / 3.0f), 12, 96);
}

void quad(DrawNode* node, const Vec2& a, const Vec2& b, const Vec2& c, const Vec2& d,
          const Color4F& ca, const Color4F& cb, const Color4F& cc, const Color4F& cd) {
    const Vec2 first[3]         = {a, b, c};
    const Color4F firstTint[3]  = {ca, cb, cc};
    const Vec2 second[3]        = {a, c, d};
    const Color4F secondTint[3] = {ca, cc, cd};
    node->drawColoredTriangle(first, firstTint);
    node->drawColoredTriangle(second, secondTint);
}

// A convex outline given twice — once pulled in by half a pixel, once pushed
// out by half a pixel, point for point. The inside is filled solid as a fan
// from `centre`; the strip between the two fades out.
void fillWithRim(DrawNode* node, const Vec2& centre, const std::vector<Vec2>& inner,
                 const std::vector<Vec2>& outer, const Color4F& colour) {
    const Color4F none = clear(colour);
    const std::size_t count = inner.size();
    for (std::size_t i = 0; i < count; ++i) {
        const std::size_t next = (i + 1) % count;
        node->drawTriangle(centre, inner[i], inner[next], colour);
        quad(node, inner[i], outer[i], outer[next], inner[next], colour, none, none, colour);
    }
}

// The outline of a rounded rectangle, counter-clockwise, with its corner
// radius grown (or shrunk) by `offset`. Growing the radius around the same
// corner centres is exactly an outward offset of the whole outline.
std::vector<Vec2> roundedOutline(const Vec2& origin, const Vec2& size, float radius, float offset,
                                 int perCorner) {
    const float r = std::max(0.0f, radius + offset);
    // Where the corner arcs are centred: fixed by the nominal radius.
    const Vec2 centres[4] = {
        {origin.x + size.x - radius, origin.y + radius},           // bottom right
        {origin.x + size.x - radius, origin.y + size.y - radius},  // top right
        {origin.x + radius, origin.y + size.y - radius},           // top left
        {origin.x + radius, origin.y + radius},                    // bottom left
    };
    std::vector<Vec2> points;
    points.reserve(static_cast<std::size_t>(4 * (perCorner + 1)));
    for (int corner = 0; corner < 4; ++corner) {
        const float start = -kPi / 2.0f + static_cast<float>(corner) * kPi / 2.0f;
        for (int step = 0; step <= perCorner; ++step) {
            const float angle = start + kPi / 2.0f * static_cast<float>(step) / static_cast<float>(perCorner);
            points.emplace_back(centres[corner].x + r * std::cos(angle),
                                centres[corner].y + r * std::sin(angle));
        }
    }
    return points;
}

}  // namespace

float pixel() {
    const float scale = Director::getInstance()->getContentScaleFactor();
    return scale > 0.0f ? 1.0f / scale : 1.0f;
}

void rect(DrawNode* node, const Vec2& origin, const Vec2& size, const Color4F& colour) {
    // Axis-aligned, so its edges fall on pixel boundaries and need no rim.
    const Vec2 a = origin;
    const Vec2 b(origin.x + size.x, origin.y);
    const Vec2 c(origin.x + size.x, origin.y + size.y);
    const Vec2 d(origin.x, origin.y + size.y);
    node->drawTriangle(a, b, c, colour);
    node->drawTriangle(a, c, d, colour);
}

void circle(DrawNode* node, const Vec2& centre, float radius, const Color4F& colour) {
    if (radius <= 0.0f)
        return;
    const float half = pixel() * 0.5f;
    const int count  = segmentsFor(radius);

    std::vector<Vec2> inner, outer;
    inner.reserve(static_cast<std::size_t>(count));
    outer.reserve(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
        const float angle = 2.0f * kPi * static_cast<float>(i) / static_cast<float>(count);
        const Vec2 unit(std::cos(angle), std::sin(angle));
        inner.push_back(centre + unit * std::max(0.0f, radius - half));
        outer.push_back(centre + unit * (radius + half));
    }
    fillWithRim(node, centre, inner, outer, colour);
}

void ring(DrawNode* node, const Vec2& centre, float innerRadius, float outerRadius, const Color4F& colour) {
    if (outerRadius <= innerRadius)
        return;
    const float half   = pixel() * 0.5f;
    const int count    = segmentsFor(outerRadius);
    const Color4F none = clear(colour);

    // Four concentric circles: fade in, solid band, fade out.
    const float radii[4] = {std::max(0.0f, innerRadius - half), innerRadius + half, outerRadius - half,
                            outerRadius + half};
    for (int i = 0; i < count; ++i) {
        const float a0 = 2.0f * kPi * static_cast<float>(i) / static_cast<float>(count);
        const float a1 = 2.0f * kPi * static_cast<float>(i + 1) / static_cast<float>(count);
        const Vec2 u0(std::cos(a0), std::sin(a0)), u1(std::cos(a1), std::sin(a1));
        const auto at = [&](const Vec2& unit, int which) { return centre + unit * radii[which]; };

        quad(node, at(u0, 0), at(u0, 1), at(u1, 1), at(u1, 0), none, colour, colour, none);
        quad(node, at(u0, 1), at(u0, 2), at(u1, 2), at(u1, 1), colour, colour, colour, colour);
        quad(node, at(u0, 2), at(u0, 3), at(u1, 3), at(u1, 2), colour, none, none, colour);
    }
}

void glow(DrawNode* node, const Vec2& centre, float radius, const Color4F& colour) {
    if (radius <= 0.0f)
        return;
    const int count    = segmentsFor(radius);
    const Color4F none = clear(colour);
    // A solid heart, so the colour is unmistakable under the piece, then the fade.
    const float core = radius * 0.35f;
    for (int i = 0; i < count; ++i) {
        const float a0 = 2.0f * kPi * static_cast<float>(i) / static_cast<float>(count);
        const float a1 = 2.0f * kPi * static_cast<float>(i + 1) / static_cast<float>(count);
        const Vec2 u0(std::cos(a0), std::sin(a0)), u1(std::cos(a1), std::sin(a1));
        node->drawTriangle(centre, centre + u0 * core, centre + u1 * core, colour);
        quad(node, centre + u0 * core, centre + u0 * radius, centre + u1 * radius, centre + u1 * core,
             colour, none, none, colour);
    }
}

void roundedRect(DrawNode* node, const Vec2& origin, const Vec2& size, float radius, const Color4F& colour) {
    if (size.x <= 0.0f || size.y <= 0.0f)
        return;
    radius = std::min({radius, size.x * 0.5f, size.y * 0.5f});
    if (radius <= pixel()) {
        rect(node, origin, size, colour);
        return;
    }
    const float half    = pixel() * 0.5f;
    const int perCorner = std::max(4, segmentsFor(radius) / 4);
    const Vec2 centre(origin.x + size.x * 0.5f, origin.y + size.y * 0.5f);
    fillWithRim(node, centre, roundedOutline(origin, size, radius, -half, perCorner),
                roundedOutline(origin, size, radius, half, perCorner), colour);
}

void roundedRect(DrawNode* node, const Vec2& origin, const Vec2& size, float radius, const Color4F& colour,
                 float border, const Color4F& borderColour) {
    // The border is the whole shape in the border colour with the fill laid
    // over it, inset. Two fills instead of a stroke: no joins to get wrong.
    roundedRect(node, origin, size, radius, borderColour);
    roundedRect(node, Vec2(origin.x + border, origin.y + border),
                Vec2(size.x - 2.0f * border, size.y - 2.0f * border), std::max(0.0f, radius - border),
                colour);
}

void arrow(DrawNode* node, const Vec2& from, const Vec2& to, float width, const Color4F& colour) {
    const Vec2 delta   = to - from;
    const float length = delta.length();
    if (length < 1.0f)
        return;
    const Vec2 along = delta / length;
    const Vec2 side(-along.y, along.x);

    const float headLength = std::min(length * 0.6f, width * 2.6f);
    const float headWidth  = width * 2.4f;
    const Vec2 neck        = to - along * headLength;

    // Shaft: a quad with a rim down each long side.
    const float half   = width * 0.5f;
    const float rim    = pixel();
    const Color4F none = clear(colour);
    const Vec2 a = from + side * half, b = neck + side * half;
    const Vec2 c = neck - side * half, d = from - side * half;
    quad(node, a, b, c, d, colour, colour, colour, colour);
    quad(node, a, a + side * rim, b + side * rim, b, colour, none, none, colour);
    quad(node, d, d - side * rim, c - side * rim, c, colour, none, none, colour);

    // Head: a triangle, with the same rim on its two slanted edges.
    const Vec2 left = neck + side * (headWidth * 0.5f), right = neck - side * (headWidth * 0.5f);
    node->drawTriangle(left, to, right, colour);
    const Vec2 leftOut  = (side + along * 0.4f).getNormalized() * rim;
    const Vec2 rightOut = (along * 0.4f - side).getNormalized() * rim;
    quad(node, left, left + leftOut, to + leftOut, to, colour, none, none, colour);
    quad(node, right, right + rightOut, to + rightOut, to, colour, none, none, colour);
}

}  // namespace shapes
