// Filled shapes with soft edges, drawn into an ax::DrawNode.
//
// DrawNode's own circles and polygons are hard-edged: its fragment shader is a
// step function, and on a 1x display a twelve-pixel dot drawn that way is a
// visibly jagged dot. Rather than depend on multisampling — which the window
// may or may not get, depending on the graphics backend — every shape here
// carries its own antialiasing: the body is drawn solid and slightly small,
// and a one-pixel rim around it fades from the fill colour to nothing. That
// works identically on OpenGL and Metal, at any scale.
//
// Everything is built from DrawNode::drawTriangle / drawColoredTriangle, the
// two calls that are plain vertex data and nothing else.
#pragma once

#include "axmol.h"

namespace shapes {

// One physical pixel, in design units. The rim is this wide.
float pixel();

void rect(ax::DrawNode* node, const ax::Vec2& origin, const ax::Vec2& size, const ax::Color4F& colour);

void circle(ax::DrawNode* node, const ax::Vec2& centre, float radius, const ax::Color4F& colour);

// A circular band between two radii.
void ring(ax::DrawNode* node, const ax::Vec2& centre, float inner, float outer, const ax::Color4F& colour);

// A disc that fades from `colour` at the centre to transparent at the edge.
void glow(ax::DrawNode* node, const ax::Vec2& centre, float radius, const ax::Color4F& colour);

void roundedRect(ax::DrawNode* node, const ax::Vec2& origin, const ax::Vec2& size, float radius,
                 const ax::Color4F& colour);

// A rounded rectangle with a border: `border` wide, in `borderColour`.
void roundedRect(ax::DrawNode* node, const ax::Vec2& origin, const ax::Vec2& size, float radius,
                 const ax::Color4F& colour, float border, const ax::Color4F& borderColour);

// A straight arrow from one point to another, `width` thick in the shaft.
void arrow(ax::DrawNode* node, const ax::Vec2& from, const ax::Vec2& to, float width,
           const ax::Color4F& colour);

}  // namespace shapes
