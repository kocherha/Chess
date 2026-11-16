// The one question the game asks before it starts: against whom, as which
// colour, and how strong.
//
// How strong is asked twice over. Eight buttons, one per level, for whoever
// wants a name to pick; and under them a slider that goes from the first level
// to the last through everything between, for whoever has found one level too
// easy and the next too hard.
#pragma once

#include "Dialog.h"
#include "Slider.h"

#include "chess/Difficulty.h"

#include "axmol.h"

#include <array>
#include <functional>

struct GameSetup {
    enum class Opponent { Computer, Human };
    enum class Side { White, Black, Random };

    Opponent opponent = Opponent::Computer;
    Side     side     = Side::White;
    // Where on the scale of chess/Difficulty.h the computer plays: one of the
    // eight levels, or anywhere between two of them.
    int      strength = chess::defaultStrength();
};

class NewGameDialog : public Dialog
{
public:
    static NewGameDialog* create();

    // Shows the dialog with `setup` preselected. `canCancel` is false for the
    // very first game, when there is nothing to go back to. Without an engine
    // only the two-player game is on offer. The Elo range is what the engine
    // said it supports, so the descriptions can quote the rating it will
    // really be asked for.
    void open(const GameSetup& setup, bool canCancel, bool engineAvailable, int eloMin, int eloMax);

    void layout(const ax::Vec2& screen) override;
    void keyPressed(ax::EventKeyboard::KeyCode code) override;

    // The slider is not a button, so the pointer is offered to it first.
    bool mouseMove(const ax::Vec2& point) override;
    void mouseDown(const ax::Vec2& point) override;
    void mouseUp(const ax::Vec2& point) override;

    std::function<void(const GameSetup&)> onPlay;
    std::function<void()>                 onCancel;
    // The other mode, from here: before the first game there is no board yet,
    // and so no panel with the switch on it.
    std::function<void()>                 onPuzzles;

private:
    bool init() override;
    void refresh();
    void choose(GameSetup::Opponent opponent);
    void choose(GameSetup::Side side);
    void chooseLevel(int number);
    void chooseStrength(int strength);

    GameSetup _setup;
    bool _canCancel = false, _engineAvailable = true;
    int  _eloMin = 0, _eloMax = 0;
    ax::Vec2 _screen;
    float    _textWidth = 0.0f;  // how wide a line of running text may be

    ax::DrawNode* _back = nullptr;
    ax::Label *_title = nullptr, *_opponentTitle = nullptr, *_sideTitle = nullptr, *_levelTitle = nullptr;
    ax::Label *_levelName = nullptr, *_levelMeasure = nullptr, *_levelDescription = nullptr, *_notice = nullptr;

    Button *_computer = nullptr, *_human = nullptr;
    Button *_white = nullptr, *_random = nullptr, *_black = nullptr;
    std::array<Button*, 8> _levels{};
    Slider* _strength = nullptr;
    Button *_play = nullptr, *_cancel = nullptr, *_puzzles = nullptr;
};
