// "Are you sure?" — asked before something that cannot be taken back, which in
// this game is one thing: walking away from a game that is still being played.
//
// The button that stays is the one that is lit, and the one both Enter and Esc
// press: a dialog that exists to prevent an accident should not be dismissed
// into the accident by a stray key.
#pragma once

#include "Dialog.h"

#include "axmol.h"

#include <functional>
#include <string>

class ConfirmDialog : public Dialog
{
public:
    static ConfirmDialog* create();

    // Asks. `proceed` is what the button that goes ahead says; `onProceed`
    // runs if it is pressed, `onStay` if the other one is.
    void open(const std::string& title, const std::string& message, const std::string& proceed,
              std::function<void()> onProceed, std::function<void()> onStay = {});

    // The two answers, for whoever needs to give one without a click.
    void proceed();
    void stay();

    void layout(const ax::Vec2& screen) override;
    void keyPressed(ax::EventKeyboard::KeyCode code) override;

private:
    bool init() override;

    ax::Vec2    _screen;
    std::string _text;  // the message as given; it is wrapped again whenever the width changes
    std::function<void()> _onProceed, _onStay;

    ax::DrawNode* _back    = nullptr;
    ax::Label*    _title   = nullptr;
    ax::Label*    _message = nullptr;
    Button *_go = nullptr, *_keep = nullptr;
};
