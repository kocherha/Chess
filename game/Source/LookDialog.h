// Choosing what the pieces and the board look like.
//
// The dialog shows what is being chosen: a strip of board with all twelve
// pieces on it, in whatever is picked at the moment. A choice takes effect
// the instant it is made — there is nothing to confirm and nothing to cancel,
// only "done".
#pragma once

#include "Dialog.h"

#include "axmol.h"

#include <functional>
#include <vector>

class LookDialog : public Dialog
{
public:
    static LookDialog* create();

    void open();

    void layout(const ax::Vec2& screen) override;
    void keyPressed(ax::EventKeyboard::KeyCode code) override;

    // Something was chosen. look:: already has it; whoever draws pieces or a
    // board should draw them again.
    std::function<void()> onChange;
    std::function<void()> onDone;

private:
    bool init() override;
    void choosePieces(int index);
    void chooseBoard(int index);
    void refresh();  // which buttons are lit, and the preview

    ax::Vec2 _screen;
    ax::Vec2 _previewOrigin;  // bottom-left corner of the strip's squares
    float    _previewSquare = 0.0f;

    ax::DrawNode* _back    = nullptr;
    ax::DrawNode* _preview = nullptr;
    ax::Node*     _pieces  = nullptr;
    ax::Label *_title = nullptr, *_piecesTitle = nullptr, *_boardTitle = nullptr;

    std::vector<Button*> _pieceButtons, _boardButtons;
    Button* _done = nullptr;
};
