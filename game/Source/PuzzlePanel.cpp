#include "PuzzlePanel.h"

#include "Shapes.h"
#include "Strings.h"
#include "Text.h"
#include "Theme.h"

#include <algorithm>
#include <cmath>

using namespace ax;
using namespace chess;

namespace {

constexpr float kPadding = 20.0f;
constexpr float kGap     = 8.0f;
constexpr float kButton  = 36.0f;   // the big buttons at the bottom
constexpr float kSmall   = 28.0f;   // the choices in the editor's rows
constexpr float kLabelColumn = 96.0f;

constexpr float kDetailPoints = 13.0f;

}  // namespace

PuzzlePanel* PuzzlePanel::create() {
    auto* node = new PuzzlePanel();
    if (node->init()) {
        node->autorelease();
        return node;
    }
    delete node;
    return nullptr;
}

Label* PuzzlePanel::addLabel(float points, const Color3B& colour) {
    Label* label = typeset::label("", points, colour);
    label->setAnchorPoint(Vec2(0.0f, 0.5f));
    addChild(label);
    return label;
}

bool PuzzlePanel::init() {
    if (!Node::init())
        return false;

    _back = DrawNode::create();
    addChild(_back);

    _title = addLabel(26.0f, theme::kInk);
    _title->setString(text::kTitle);
    typeset::embolden(_title);
    _heading  = addLabel(17.0f, theme::kInk);
    _headline = addLabel(18.0f, theme::kInk);
    _detail   = addLabel(kDetailPoints, theme::kDim);
    _detail->setAnchorPoint(Vec2(0.0f, 1.0f));

    const auto button = [this](const char* text, Button::Style style = Button::Style::Plain, float points = 13.5f) {
        Button* made = Button::create(text, style, points);
        addChild(made);
        return made;
    };

    looks       = button(text::kLooks, Button::Style::Plain, 14.0f);
    modeGame    = button(text::kModeGame, Button::Style::Choice, 14.0f);
    modePuzzles = button(text::kModePuzzles, Button::Style::Choice, 14.0f);
    modePuzzles->setChosen(true);
    collection  = button(text::kCollection, Button::Style::Plain, 13.0f);

    // --- the editor ---
    _toMoveLabel   = addLabel(13.0f, theme::kDim);
    _castlingLabel = addLabel(13.0f, theme::kDim);
    _whiteLabel    = addLabel(13.0f, theme::kDim);
    _blackLabel    = addLabel(13.0f, theme::kDim);
    _passantLabel  = addLabel(13.0f, theme::kDim);
    _toMoveLabel->setString(text::kToMove);
    _castlingLabel->setString(text::kCastling);
    _whiteLabel->setString(std::string("  ") + text::kWhite);
    _blackLabel->setString(std::string("  ") + text::kBlack);
    _passantLabel->setString(text::kEnPassant);
    _fen = addLabel(11.5f, theme::kFaint);
    _fen->setAnchorPoint(Vec2(0.0f, 1.0f));
    _help = addLabel(12.0f, theme::kFaint);
    _help->setAnchorPoint(Vec2(0.0f, 1.0f));

    _white = button(text::kWhite, Button::Style::Choice);
    _black = button(text::kBlack, Button::Style::Choice);
    _white->onClick = [this] { if (onSideToMove) onSideToMove(Color::White); };
    _black->onClick = [this] { if (onSideToMove) onSideToMove(Color::Black); };
    for (int i = 0; i < 4; ++i) {
        _castling[static_cast<std::size_t>(i)] = button(i % 2 == 0 ? text::kCastleShort : text::kCastleLong,
                                                        Button::Style::Choice);
        _castling[static_cast<std::size_t>(i)]->onClick = [this, i] { if (onCastling) onCastling(i); };
    }
    for (std::size_t i = 0; i < _passant.size(); ++i) {
        _passant[i] = button(i == 0 ? text::kNoEnPassant : "", Button::Style::Choice);
        _passantSquares[i] = kNoSquare;
        _passant[i]->onClick = [this, i] { if (onEnPassant) onEnPassant(_passantSquares[i]); };
    }

    start    = button(text::kStartPosition);
    clear    = button(text::kClearBoard);
    flip     = button(text::kFlip);
    copyFen  = button(text::kCopyFen);
    pasteFen = button(text::kPasteFen);
    playFrom = button(text::kPlayFrom);
    saveMine = button(text::kSaveMine);
    solve    = button(text::kSolve, Button::Style::Primary, 16.0f);

    _editorOnly = {_toMoveLabel, _castlingLabel, _whiteLabel, _blackLabel, _passantLabel, _fen, _help,
                   _white, _black, start, clear, flip, copyFen, pasteFen, playFrom, saveMine, solve};
    for (Button* b : _castling)
        _editorOnly.push_back(b);
    for (Button* b : _passant)
        _editorOnly.push_back(b);

    // --- solving ---
    _note  = addLabel(12.0f, theme::kFaint);
    _goal  = addLabel(14.0f, theme::kDim);
    _moves = addLabel(15.0f, theme::kInk);
    for (Label* label : {_note, _goal, _moves})
        label->setAnchorPoint(Vec2(0.0f, 1.0f));

    next        = button(text::kNextPuzzle, Button::Style::Primary, 16.0f);
    hint        = button(text::kHint);
    solution    = button(text::kSolution);
    undo        = button(text::kUndo);
    restart     = button(text::kRestart);
    edit        = button(text::kEditPosition);
    removeMine  = button(text::kDeleteMine);
    flipSolving = button(text::kFlip);
    _solvingOnly = {_note, _goal, _moves, next, hint, solution, undo, restart, edit, removeMine, flipSolving};

    setPage(Page::Editor);
    return true;
}

void PuzzlePanel::setFrame(const Vec2& origin, const Vec2& size) {
    setPosition(origin);
    _size = size;
    layout();
}

void PuzzlePanel::setPage(Page page) {
    _page = page;
    for (Node* node : _editorOnly)
        node->setVisible(page == Page::Editor);
    for (Node* node : _solvingOnly)
        node->setVisible(page == Page::Solving);
    removeMine->setVisible(page == Page::Solving && _mine);
    flipSolving->setVisible(page == Page::Solving && !_mine);
    _heading->setString(page == Page::Editor ? text::kEditorTitle : _heading->getString());
    layout();
}

void PuzzlePanel::setStatus(const std::string& headline, const std::string& detail, Tone tone) {
    if (headline == _headlineText && detail == _detailText && tone == _tone)
        return;
    _headlineText = headline;
    _detailText   = detail;
    _tone         = tone;
    layout();
}

void PuzzlePanel::setSideToMove(Color colour) {
    _white->setChosen(colour == Color::White);
    _black->setChosen(colour == Color::Black);
}

void PuzzlePanel::setCastling(const std::array<bool, 4>& wanted, const std::array<bool, 4>& possible) {
    for (std::size_t i = 0; i < 4; ++i) {
        _castling[i]->setEnabled(possible[i]);
        _castling[i]->setChosen(wanted[i] && possible[i]);
    }
}

void PuzzlePanel::setEnPassant(const std::vector<Square>& candidates, Square chosen) {
    _passantSquares.fill(kNoSquare);
    for (std::size_t i = 0; i < candidates.size() && i + 1 < _passant.size(); ++i) {
        _passantSquares[i + 1] = candidates[i];
        _passant[i + 1]->setText(squareName(candidates[i]));
    }
    for (std::size_t i = 0; i < _passant.size(); ++i)
        _passant[i]->setChosen(_passantSquares[i] == chosen);
    _passant[0]->setEnabled(!candidates.empty());
    layout();
}

void PuzzlePanel::setFen(const std::string& fen) {
    if (fen == _fenText)
        return;
    _fenText = fen;
    layout();
}

void PuzzlePanel::setPuzzle(const std::string& title, const std::string& note, const std::string& goal) {
    _heading->setString(title);
    _noteText = note;
    _goalText = goal;
    layout();
}

void PuzzlePanel::setMoves(const std::string& moves) {
    if (moves == _movesText)
        return;
    _movesText = moves;
    layout();
}

void PuzzlePanel::setMine(bool mine) {
    _mine = mine;
    setPage(_page);
}

void PuzzlePanel::collectLive() {
    _live.clear();
    for (Button* b : {looks, modeGame, modePuzzles, collection})
        _live.add(b);
    const auto& page = _page == Page::Editor ? _editorOnly : _solvingOnly;
    for (Node* node : page)
        if (auto* b = dynamic_cast<Button*>(node); b != nullptr && b->isVisible())
            _live.add(b);
}

void PuzzlePanel::layout() {
    if (_size.x <= 0.0f)
        return;

    const float width = _size.x - 2.0f * kPadding;
    const float left  = kPadding;
    float y           = _size.y - kPadding;

    // In a short window everything is drawn a little tighter, and the en
    // passant row only appears when there is something to choose in it.
    const bool  compact = _size.y < 660.0f;
    const float button = compact ? 32.0f : kButton;
    const float small  = compact ? 26.0f : kSmall;
    const float gap    = compact ? 6.0f : kGap;

    _back->clear();
    shapes::rect(_back, Vec2::ZERO, _size, theme::kPanel);
    shapes::rect(_back, Vec2::ZERO, Vec2(1.0f, _size.y), theme::kLine);

    // The title, as on the game's panel, and the switch between the modes.
    _title->setPosition(Vec2(left, y - 15.0f));
    looks->setFrame(Vec2(left + width - 92.0f, y - 30.0f), Vec2(92.0f, 30.0f));
    y -= 42.0f;
    const float half = std::floor((width - gap) * 0.5f);
    modeGame->setFrame(Vec2(left, y - 30.0f), Vec2(half, 30.0f));
    modePuzzles->setFrame(Vec2(left + width - half, y - 30.0f), Vec2(half, 30.0f));
    y -= 30.0f + 18.0f;

    // The page's own heading, with the way to the collection at its side.
    const float collectionWidth = 124.0f;
    collection->setFrame(Vec2(left + width - collectionWidth, y - 28.0f), Vec2(collectionWidth, 28.0f));
    _heading->setPosition(Vec2(left, y - 14.0f));
    // A long title of a problem gives way to the button rather than run under it.
    const float headingRoom = width - collectionWidth - 10.0f;
    _heading->setScale(1.0f);
    if (_heading->getContentSize().width > headingRoom && _heading->getContentSize().width > 0.0f)
        _heading->setScale(headingRoom / _heading->getContentSize().width);
    y -= 28.0f + (compact ? 8.0f : 12.0f);

    // The buttons at the bottom, from the bottom up; the space above them is
    // what the rest of the page has.
    float bottom    = kPadding;
    const float third = std::floor((width - 2.0f * gap) / 3.0f);
    const auto row2 = [&](Button* a, Button* b) {
        a->setFrame(Vec2(left, bottom), Vec2(half, button));
        b->setFrame(Vec2(left + width - half, bottom), Vec2(half, button));
        bottom += button + gap;
    };

    if (_page == Page::Editor) {
        solve->setFrame(Vec2(left, bottom), Vec2(width, button + 4.0f));
        bottom += button + 4.0f + gap;
        row2(playFrom, saveMine);
        row2(copyFen, pasteFen);
        start->setFrame(Vec2(left, bottom), Vec2(third, button));
        clear->setFrame(Vec2(left + third + gap, bottom), Vec2(third, button));
        flip->setFrame(Vec2(left + width - third, bottom), Vec2(third, button));
        bottom += button + (compact ? 8.0f : 14.0f);

        // Rows of choices: a label on the left, buttons after it.
        const float column = left + kLabelColumn;
        const float room   = width - kLabelColumn;
        const auto rowLabel = [&](Label* label) { label->setPosition(Vec2(left, y - small * 0.5f)); };

        rowLabel(_toMoveLabel);
        const float sideWidth = std::floor((room - gap) * 0.5f);
        _white->setFrame(Vec2(column, y - small), Vec2(sideWidth, small));
        _black->setFrame(Vec2(column + sideWidth + gap, y - small), Vec2(sideWidth, small));
        y -= small + gap;

        _castlingLabel->setPosition(Vec2(left, y - 8.0f));
        y -= compact ? 16.0f : 22.0f;
        for (int row = 0; row < 2; ++row) {
            rowLabel(row == 0 ? _whiteLabel : _blackLabel);
            for (int i = 0; i < 2; ++i)
                _castling[static_cast<std::size_t>(row * 2 + i)]->setFrame(
                    Vec2(column + static_cast<float>(i) * (sideWidth + gap), y - small), Vec2(sideWidth, small));
            y -= small + gap;
        }

        const bool passantRow = !compact || _passantSquares[1] != kNoSquare;
        _passantLabel->setVisible(passantRow);
        rowLabel(_passantLabel);
        int shown = 0;
        for (std::size_t i = 0; i < _passant.size(); ++i) {
            const bool used = passantRow && (i == 0 || _passantSquares[i] != kNoSquare);
            _passant[i]->setVisible(_page == Page::Editor && used);
            if (!used)
                continue;
            const float cell = 46.0f;
            _passant[i]->setFrame(Vec2(column + static_cast<float>(shown) * (cell + 6.0f), y - small),
                                  Vec2(cell, small));
            ++shown;
        }
        if (passantRow)
            y -= small + (compact ? 10.0f : 16.0f);
    } else {
        _note->setString(typeset::wrap(_noteText, 12.0f, width));
        _note->setPosition(Vec2(left, y + 6.0f));
        if (!_noteText.empty())
            y -= _note->getContentSize().height + 6.0f;
        _goal->setString(typeset::wrap(_goalText, 14.0f, width));
        _goal->setPosition(Vec2(left, y));
        y -= _goal->getContentSize().height + 16.0f;

        next->setFrame(Vec2(left, bottom), Vec2(width, button + 4.0f));
        bottom += button + 4.0f + gap;
        row2(hint, solution);
        row2(undo, restart);
        row2(edit, _mine ? removeMine : flipSolving);
        bottom += 6.0f;
    }

    // The card: one line in the middle, or a headline and paragraphs under it.
    _headline->setString(_headlineText);
    _headline->setColor(_tone == Tone::Good      ? theme::kGood
                        : _tone == Tone::Warning ? theme::kWarn
                        : _tone == Tone::Bad     ? theme::kBad
                                                 : theme::kInk);
    _detail->setString(typeset::wrapParagraphs(_detailText, kDetailPoints, width - 28.0f));
    const bool twoLines = !_detailText.empty();
    // As tall as it needs, but never pushing into the buttons: what does not
    // fit is cut at the card's bottom edge (the first lines matter most).
    const float wanted   = twoLines ? 40.0f + _detail->getContentSize().height + 12.0f : 56.0f;
    const float reserve  = compact ? 8.0f : _page == Page::Editor ? 34.0f : 40.0f;  // for the FEN or the moves
    const float room     = std::max(56.0f, y - bottom - reserve);
    const float card     = std::min(wanted, room);
    const Color4F border = _tone == Tone::Good      ? theme::kAccent
                           : _tone == Tone::Warning ? theme::rgb(170, 130, 70)
                           : _tone == Tone::Bad     ? theme::rgb(176, 72, 64)
                                                    : theme::kLine;
    shapes::roundedRect(_back, Vec2(left, y - card), Vec2(width, card), 10.0f, theme::kCard, 1.0f, border);
    _headline->setPosition(Vec2(left + 14.0f, y - (twoLines ? 22.0f : card * 0.5f)));
    _detail->setPosition(Vec2(left + 14.0f, y - 38.0f));
    // Lines that would hang out of the card are not drawn.
    _detail->setVisible(twoLines && _detail->getContentSize().height <= card - 44.0f + 1.0f);
    if (twoLines && !_detail->isVisible()) {
        // Too long: keep as many whole paragraphs as fit.
        std::string kept, rest = _detailText;
        while (!rest.empty()) {
            const std::size_t end  = rest.find('\n');
            const std::string para = rest.substr(0, end);
            const std::string trial = kept.empty() ? para : kept + "\n" + para;
            _detail->setString(typeset::wrapParagraphs(trial, kDetailPoints, width - 28.0f));
            if (_detail->getContentSize().height > card - 44.0f)
                break;
            kept = trial;
            rest = end == std::string::npos ? "" : rest.substr(end + 1);
        }
        _detail->setString(typeset::wrapParagraphs(kept, kDetailPoints, width - 28.0f));
        _detail->setVisible(!kept.empty());
        if (kept.empty())
            _headline->setPositionY(y - card * 0.5f);
    }
    y -= card + 12.0f;

    if (_page == Page::Editor) {
        // The position in FEN, small, and — if there is room — how to use the editor.
        _fen->setString(typeset::wrap(_fenText, 11.5f, width));
        _fen->setPosition(Vec2(left, y));
        y -= _fen->getContentSize().height + 10.0f;
        _help->setString(typeset::wrap(text::kEditorHelp, 12.0f, width));
        _help->setPosition(Vec2(left, y));
        _help->setVisible(y - _help->getContentSize().height > bottom);
        _fen->setVisible(_fen->getPositionY() - _fen->getContentSize().height > bottom - 4.0f);
    } else {
        _moves->setString(typeset::wrap(_movesText, 15.0f, width));
        _moves->setPosition(Vec2(left, y));
        _moves->setVisible(y - _moves->getContentSize().height > bottom - 4.0f);
    }

    collectLive();
}
