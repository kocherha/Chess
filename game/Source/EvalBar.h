// The strip beside the board that says who is better and by how much.
//
// It is told a score — in white's favour or black's — and shows it the way
// chess sites do: a bar that is half white and half black when the game is
// level and fills with one colour as that side pulls ahead, with the number
// written at the leading side's end.
#pragma once

#include "axmol.h"

#include <string>

class EvalBar : public ax::Node
{
public:
    // Always from white's point of view: positive is good for white.
    struct Score {
        enum class Kind {
            Unknown,     // nothing to go on yet
            Centipawns,  // `value` hundredths of a pawn
            Mate,        // mate in `value` moves; negative: black mates
            WhiteWon,
            BlackWon,
            Drawn,
        };
        Kind kind  = Kind::Unknown;
        int  value = 0;

        bool operator==(const Score&) const = default;
    };

    static EvalBar* create();

    void setFrame(const ax::Vec2& origin, const ax::Vec2& size);

    // White's share is at the bottom, like white's pieces — unless the board
    // has been turned round.
    void setFlipped(bool flipped);

    void show(const Score& score);

    // Moves the bar towards what it was last told to show. Returns true while
    // it still has some way to go, so the scene knows to keep drawing.
    bool step(float delta);

    // How much of the bar is white for a given score, 0..1. An evaluation is
    // not linear in what it means: the step from level to a pawn up matters
    // far more than the step from eight pawns up to nine. This is the usual
    // logistic "winning chances" curve — chess::winChance, the same one a
    // move is marked by.
    static float whiteShare(const Score& score);
    // "0.3", "12", "#4", "1–0": the size of the advantage, without a sign —
    // which end of the bar it is written at says whose it is.
    static std::string label(const Score& score);

private:
    bool init() override;
    void redraw();

    ax::Vec2 _size;
    bool  _flipped = false;
    Score _score;
    float _target = 0.5f;  // where the boundary should be
    float _shown  = 0.5f;  // where it is drawn

    ax::DrawNode* _bar   = nullptr;
    ax::Label*    _label = nullptr;
};
