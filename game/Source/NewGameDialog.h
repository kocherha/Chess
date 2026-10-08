// The one question the game asks before it starts: against whom, as which
// colour, and how strong.
#pragma once

#include "Dialog.h"

#include "chess/Difficulty.h"

#include "axmol.h"

#include <array>
#include <functional>

struct GameSetup {
    enum class Opponent { Computer, Human };
    enum class Side { White, Black, Random };

    Opponent opponent = Opponent::Computer;
    Side     side     = Side::White;
    int      level    = chess::defaultLevelNumber();
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

    std::function<void(const GameSetup&)> onPlay;
    std::function<void()>                 onCancel;

private:
    bool init() override;
    void refresh();
    void choose(GameSetup::Opponent opponent);
    void choose(GameSetup::Side side);
    void chooseLevel(int number);

    GameSetup _setup;
    bool _canCancel = false, _engineAvailable = true;
    int  _eloMin = 0, _eloMax = 0;
    ax::Vec2 _screen;
    float    _textWidth = 0.0f;  // how wide a line of running text may be

    ax::DrawNode* _back = nullptr;
    ax::Label *_title = nullptr, *_opponentTitle = nullptr, *_sideTitle = nullptr, *_levelTitle = nullptr;
    ax::Label *_levelName = nullptr, *_levelDescription = nullptr, *_notice = nullptr;

    Button *_computer = nullptr, *_human = nullptr;
    Button *_white = nullptr, *_random = nullptr, *_black = nullptr;
    std::array<Button*, 8> _levels{};
    Button *_play = nullptr, *_cancel = nullptr;
};
