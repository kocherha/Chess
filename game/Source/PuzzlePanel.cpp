#include "PuzzlePanel.h"

#include "Shapes.h"
#include "Strings.h"
#include "Text.h"
#include "Theme.h"

#include <algorithm>
#include <cmath>
#include <utility>

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
    const auto before = _passantSquares;
    _passantSquares.fill(kNoSquare);
    for (std::size_t i = 0; i < candidates.size() && i + 1 < _passant.size(); ++i) {
        _passantSquares[i + 1] = candidates[i];
        _passant[i + 1]->setText(squareName(candidates[i]));
    }
    for (std::size_t i = 0; i < _passant.size(); ++i)
        _passant[i]->setChosen(_passantSquares[i] == chosen);
    _passant[0]->setEnabled(!candidates.empty());
    // Which buttons the row has is a matter of layout; which is lit is not.
    // The editor calls this after every change on the board, and most of
    // them leave the squares as they were.
    if (_passantSquares != before)
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
    const Metrics m = metrics();

    _back->clear();
    shapes::rect(_back, Vec2::ZERO, _size, theme::kPanel);
    shapes::rect(_back, Vec2::ZERO, Vec2(1.0f, _size.y), theme::kLine);

    float y = layoutHeader(m);
    float bottom;
    if (_page == Page::Editor) {
        bottom = layoutEditorButtons(m);
        y      = layoutEditorChoices(m, y);
    } else {
        bottom = layoutSolvingButtons(m);
        y      = layoutGoal(m, y);
    }
    y = layoutCard(m, y, bottom);
    layoutFooter(m, y, bottom);

    collectLive();
}

PuzzlePanel::Metrics PuzzlePanel::metrics() const {
    Metrics m;
    m.width   = _size.x - 2.0f * kPadding;
    m.left    = kPadding;
    // In a short window everything is drawn a little tighter, and the en
    // passant row only appears when there is something to choose in it.
    m.compact = _size.y < 660.0f;
    m.button  = m.compact ? 32.0f : kButton;
    m.small   = m.compact ? 26.0f : kSmall;
    m.gap     = m.compact ? 6.0f : kGap;
    m.half    = std::floor((m.width - m.gap) * 0.5f);
    m.third   = std::floor((m.width - 2.0f * m.gap) / 3.0f);
    return m;
}

// The title, as on the game's panel, the switch between the modes, and the
// page's own heading with the way to the collection at its side.
float PuzzlePanel::layoutHeader(const Metrics& m) {
    float y = _size.y - kPadding;
    _title->setPosition(Vec2(m.left, y - 15.0f));
    looks->setFrame(Vec2(m.left + m.width - 92.0f, y - 30.0f), Vec2(92.0f, 30.0f));
    y -= 42.0f;
    modeGame->setFrame(Vec2(m.left, y - 30.0f), Vec2(m.half, 30.0f));
    modePuzzles->setFrame(Vec2(m.left + m.width - m.half, y - 30.0f), Vec2(m.half, 30.0f));
    y -= 30.0f + 18.0f;

    const float collectionWidth = 124.0f;
    collection->setFrame(Vec2(m.left + m.width - collectionWidth, y - 28.0f), Vec2(collectionWidth, 28.0f));
    _heading->setPosition(Vec2(m.left, y - 14.0f));
    // A long title of a problem gives way to the button rather than run under it.
    const float headingRoom = m.width - collectionWidth - 10.0f;
    _heading->setScale(1.0f);
    if (_heading->getContentSize().width > headingRoom && _heading->getContentSize().width > 0.0f)
        _heading->setScale(headingRoom / _heading->getContentSize().width);
    return y - (28.0f + (m.compact ? 8.0f : 12.0f));
}

float PuzzlePanel::layoutEditorButtons(const Metrics& m) {
    float bottom = kPadding;
    solve->setFrame(Vec2(m.left, bottom), Vec2(m.width, m.button + 4.0f));
    bottom += m.button + 4.0f + m.gap;
    for (auto [a, b] : {std::pair{playFrom, saveMine}, std::pair{copyFen, pasteFen}}) {
        a->setFrame(Vec2(m.left, bottom), Vec2(m.half, m.button));
        b->setFrame(Vec2(m.left + m.width - m.half, bottom), Vec2(m.half, m.button));
        bottom += m.button + m.gap;
    }
    start->setFrame(Vec2(m.left, bottom), Vec2(m.third, m.button));
    clear->setFrame(Vec2(m.left + m.third + m.gap, bottom), Vec2(m.third, m.button));
    flip->setFrame(Vec2(m.left + m.width - m.third, bottom), Vec2(m.third, m.button));
    return bottom + m.button + (m.compact ? 8.0f : 14.0f);
}

float PuzzlePanel::layoutSolvingButtons(const Metrics& m) {
    float bottom = kPadding;
    next->setFrame(Vec2(m.left, bottom), Vec2(m.width, m.button + 4.0f));
    bottom += m.button + 4.0f + m.gap;
    for (auto [a, b] : {std::pair{hint, solution}, std::pair{undo, restart},
                        std::pair{edit, _mine ? removeMine : flipSolving}}) {
        a->setFrame(Vec2(m.left, bottom), Vec2(m.half, m.button));
        b->setFrame(Vec2(m.left + m.width - m.half, bottom), Vec2(m.half, m.button));
        bottom += m.button + m.gap;
    }
    return bottom + 6.0f;
}

// Rows of choices: a label on the left, buttons after it — the side to move,
// castling for each side, the en passant square.
float PuzzlePanel::layoutEditorChoices(const Metrics& m, float y) {
    const float column    = m.left + kLabelColumn;
    const float room      = m.width - kLabelColumn;
    const float sideWidth = std::floor((room - m.gap) * 0.5f);
    const auto  rowLabel  = [&](Label* label) { label->setPosition(Vec2(m.left, y - m.small * 0.5f)); };

    rowLabel(_toMoveLabel);
    _white->setFrame(Vec2(column, y - m.small), Vec2(sideWidth, m.small));
    _black->setFrame(Vec2(column + sideWidth + m.gap, y - m.small), Vec2(sideWidth, m.small));
    y -= m.small + m.gap;

    _castlingLabel->setPosition(Vec2(m.left, y - 8.0f));
    y -= m.compact ? 16.0f : 22.0f;
    for (int row = 0; row < 2; ++row) {
        rowLabel(row == 0 ? _whiteLabel : _blackLabel);
        for (int i = 0; i < 2; ++i)
            _castling[static_cast<std::size_t>(row * 2 + i)]->setFrame(
                Vec2(column + static_cast<float>(i) * (sideWidth + m.gap), y - m.small), Vec2(sideWidth, m.small));
        y -= m.small + m.gap;
    }

    const bool passantRow = !m.compact || _passantSquares[1] != kNoSquare;
    _passantLabel->setVisible(passantRow);
    rowLabel(_passantLabel);
    int shown = 0;
    for (std::size_t i = 0; i < _passant.size(); ++i) {
        const bool used = passantRow && (i == 0 || _passantSquares[i] != kNoSquare);
        _passant[i]->setVisible(used);
        if (!used)
            continue;
        const float cell = 46.0f;
        _passant[i]->setFrame(Vec2(column + static_cast<float>(shown) * (cell + 6.0f), y - m.small),
                              Vec2(cell, m.small));
        ++shown;
    }
    if (passantRow)
        y -= m.small + (m.compact ? 10.0f : 16.0f);
    return y;
}

// The problem's note, if it has one, and what it asks for.
float PuzzlePanel::layoutGoal(const Metrics& m, float y) {
    _note->setString(typeset::wrap(_noteText, 12.0f, m.width));
    _note->setPosition(Vec2(m.left, y + 6.0f));
    if (!_noteText.empty())
        y -= _note->getContentSize().height + 6.0f;
    _goal->setString(typeset::wrap(_goalText, 14.0f, m.width));
    _goal->setPosition(Vec2(m.left, y));
    return y - (_goal->getContentSize().height + 16.0f);
}

// The card: one line in the middle, or a headline and paragraphs under it.
float PuzzlePanel::layoutCard(const Metrics& m, float y, float bottom) {
    _headline->setString(_headlineText);
    _headline->setColor(_tone == Tone::Good      ? theme::kGood
                        : _tone == Tone::Warning ? theme::kWarn
                        : _tone == Tone::Bad     ? theme::kBad
                                                 : theme::kInk);
    const float textWidth = m.width - 28.0f;
    _detail->setString(typeset::wrapParagraphs(_detailText, kDetailPoints, textWidth));
    const bool twoLines = !_detailText.empty();
    // As tall as it needs, but never pushing into the buttons: what does not
    // fit is cut at the card's bottom edge (the first lines matter most).
    const float wanted   = twoLines ? 40.0f + _detail->getContentSize().height + 12.0f : 56.0f;
    const float reserve  = m.compact ? 8.0f : _page == Page::Editor ? 34.0f : 40.0f;  // for the FEN or the moves
    const float room     = std::max(56.0f, y - bottom - reserve);
    const float card     = std::min(wanted, room);
    const Color4F border = _tone == Tone::Good      ? theme::kAccent
                           : _tone == Tone::Warning ? theme::rgb(170, 130, 70)
                           : _tone == Tone::Bad     ? theme::rgb(176, 72, 64)
                                                    : theme::kLine;
    shapes::roundedRect(_back, Vec2(m.left, y - card), Vec2(m.width, card), 10.0f, theme::kCard, 1.0f, border);
    _headline->setPosition(Vec2(m.left + 14.0f, y - (twoLines ? 22.0f : card * 0.5f)));
    _detail->setPosition(Vec2(m.left + 14.0f, y - 38.0f));
    // Lines that would hang out of the card are not drawn.
    _detail->setVisible(twoLines && _detail->getContentSize().height <= card - 44.0f + 1.0f);
    if (twoLines && !_detail->isVisible()) {
        fitDetail(card, textWidth);
        if (!_detail->isVisible())
            _headline->setPositionY(y - card * 0.5f);
    }
    return y - (card + 12.0f);
}

// The detail is too long for the card: keep as many whole paragraphs as fit.
void PuzzlePanel::fitDetail(float card, float textWidth) {
    std::string kept, rest = _detailText;
    while (!rest.empty()) {
        const std::size_t end   = rest.find('\n');
        const std::string para  = rest.substr(0, end);
        const std::string trial = kept.empty() ? para : kept + "\n" + para;
        _detail->setString(typeset::wrapParagraphs(trial, kDetailPoints, textWidth));
        if (_detail->getContentSize().height > card - 44.0f)
            break;
        kept = trial;
        rest = end == std::string::npos ? "" : rest.substr(end + 1);
    }
    _detail->setString(typeset::wrapParagraphs(kept, kDetailPoints, textWidth));
    _detail->setVisible(!kept.empty());
}

// Under the card, in what is left: in the editor the position in FEN, small,
// and — if there is room — how to use the editor; when solving, the moves.
void PuzzlePanel::layoutFooter(const Metrics& m, float y, float bottom) {
    if (_page == Page::Editor) {
        _fen->setString(typeset::wrap(_fenText, 11.5f, m.width));
        _fen->setPosition(Vec2(m.left, y));
        y -= _fen->getContentSize().height + 10.0f;
        _help->setString(typeset::wrap(text::kEditorHelp, 12.0f, m.width));
        _help->setPosition(Vec2(m.left, y));
        _help->setVisible(y - _help->getContentSize().height > bottom);
        _fen->setVisible(_fen->getPositionY() - _fen->getContentSize().height > bottom - 4.0f);
    } else {
        _moves->setString(typeset::wrap(_movesText, 15.0f, m.width));
        _moves->setPosition(Vec2(m.left, y));
        _moves->setVisible(y - _moves->getContentSize().height > bottom - 4.0f);
    }
}
