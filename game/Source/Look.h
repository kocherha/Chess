// What the pieces and the board look like, as a choice.
//
// The two are chosen separately: a set of pieces is twelve pictures in a
// folder, a board is a handful of colours and, if it is laid from real
// material, two pictures of that material.
// Any set stands on any board. Adding one of either kind is a new entry in the
// table in Look.cpp (and, for pieces, a folder of PNGs) — nothing else in the
// game knows how many there are or what they are called.
//
// The rest of the room — the panel, the buttons, the dialogs — stays as it is
// whatever is chosen here; that is Theme.h.
#pragma once

#include "chess/Types.h"

#include "axmol.h"

#include <string_view>
#include <vector>

namespace look {

struct PieceSet {
    const char* id;      // what is remembered between runs, and --pieces
    const char* name;    // what the player reads
    const char* folder;  // under Content/: twelve files, wK.png ... bP.png
};

struct BoardStyle {
    const char* id;
    const char* name;

    ax::Color4F light, dark;  // the squares
    ax::Color4F frame;        // the border around them
    ax::Color4F trim;         // a thin line between the two; alpha 0 for none
    float       corner;       // roundness of the frame: 1 is a full curve, 0 a square corner

    // Squares cut from pictures rather than painted flat: a folder under
    // Content/ with light.png and dark.png, or null. Each picture holds four
    // squares, two by two — four cuts of the same wood or stone — and every
    // square of the board is one of them, some turned over, so that no two
    // neighbours are the same piece. `light` and `dark` above are then what
    // the pictures come to on average: the coordinates are written in them,
    // and they are what shows if a picture is missing. See art/boards.py.
    const char* tiles;

    // Marks on the board. Translucent, so the square still reads through, and
    // chosen per board because they have to show against both of its colours.
    ax::Color4F lastMove, selected, target, check;
};

const std::vector<PieceSet>&   pieceSets();
const std::vector<BoardStyle>& boards();

// What is in use now.
const PieceSet&   pieces();
const BoardStyle& board();
int pieceIndex();
int boardIndex();

// Takes back what was chosen last time. Until this is called the first of
// each kind is in use.
void restore();

// Choosing. An index out of range is ignored. `keep` writes the choice down
// for the next run; the command line passes false, so that looking at a set
// once does not make it the default.
void choosePieces(int index, bool keep = true);
void chooseBoard(int index, bool keep = true);
// By id; false if there is no such thing.
bool choosePieces(std::string_view id, bool keep = true);
bool chooseBoard(std::string_view id, bool keep = true);

// A piece from the set in use, scaled to stand in a square of this size.
// Null if the picture is missing.
ax::Sprite* sprite(chess::Piece piece, float square);
// The same from a named set, for showing one that is not in use.
ax::Sprite* sprite(const PieceSet& set, chess::Piece piece, float square);

// Draws a board of `columns` x `rows` squares of side `square`, with its
// bottom-left square's corner at `origin` and a frame `frame` wide around it.
// The bottom-left square is always a dark one: that is true of a chessboard
// from either side.
//
// The frame and the flat colours go into `node`. A board laid from pictures
// also puts one sprite per square into `tiles`, which has to lie directly
// above `node` and share its coordinates; whatever was in `tiles` before is
// removed, whichever kind of board this is.
void drawBoard(ax::DrawNode* node, ax::Node* tiles, const BoardStyle& style, const ax::Vec2& origin,
               float square, int columns, int rows, float frame);

// How wide the frame of a board with squares of this size is. The same for
// every style, so that changing the board does not move anything around it.
float frameWidth(float square);

// The colour a coordinate — "a", "8" — is written in on a light square of
// the board, or a dark one: the colour of the other kind of square, so that
// the letters belong to the board. Unless the two are too alike to read one
// on the other (cream and pink): then both are written in the frame's.
ax::Color3B coordinateColour(const BoardStyle& style, bool onLight);

}  // namespace look
