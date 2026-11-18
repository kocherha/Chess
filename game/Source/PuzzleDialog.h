// The list of problems to choose from: the collection, by length of the mate,
// and the player's own. A tick beside the ones already solved.
#pragma once

#include "Dialog.h"
#include "Puzzles.h"

#include "axmol.h"

#include <functional>
#include <vector>

class PuzzleDialog : public Dialog
{
public:
    static PuzzleDialog* create();

    // Reads the lists afresh: the player may have saved or solved something
    // since it was last open.
    void open();

    void keyPressed(ax::EventKeyboard::KeyCode code) override;

    std::function<void(const Puzzle&)> onChoose;
    std::function<void()>              onCancel;

private:
    void place(const ax::Vec2& screen) override;
    bool init() override;
    void rebuild();

    struct Group {
        ax::Label*           title = nullptr;
        std::vector<Puzzle>  puzzles;
        std::vector<Button*> buttons;
    };

    ax::DrawNode* _back  = nullptr;
    ax::DrawNode* _ticks = nullptr;
    ax::Node*     _content = nullptr;  // everything that is made anew on each open
    ax::Label*    _title = nullptr;
    ax::Label*    _empty = nullptr;
    std::vector<Group> _groups;
    Button* _close = nullptr;
};
