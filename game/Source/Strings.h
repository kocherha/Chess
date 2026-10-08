// Everything the game says, in one file.
//
// The interface is in Ukrainian. Nothing else in the game contains a word a
// player reads, so putting it into another language means translating this
// file and Strings.cpp, and nothing more.
#pragma once

#include "chess/Difficulty.h"
#include "chess/Game.h"

#include <optional>
#include <string>

namespace text {

inline constexpr const char* kTitle = "Шахи";

// Buttons.
inline constexpr const char* kNewGame   = "Нова гра";
inline constexpr const char* kUndo      = "Хід назад";
inline constexpr const char* kHint      = "Підказка";
inline constexpr const char* kFlip      = "Перевернути";
inline constexpr const char* kResign    = "Здатися";
inline constexpr const char* kPlay      = "Грати";
inline constexpr const char* kCancel    = "Скасувати";
inline constexpr const char* kLooks     = "Вигляд";
inline constexpr const char* kDone      = "Готово";
inline constexpr const char* kCopyAll   = "Копіювати все";
inline constexpr const char* kCopied    = "Скопійовано";

// The new-game dialog.
inline constexpr const char* kOpponent      = "Суперник";
inline constexpr const char* kVsComputer    = "Комп'ютер";
inline constexpr const char* kVsHuman       = "Удвох за однією дошкою";
inline constexpr const char* kYourColour    = "Ваш колір";
inline constexpr const char* kWhite         = "Білі";
inline constexpr const char* kBlack         = "Чорні";
inline constexpr const char* kRandom        = "Випадково";
inline constexpr const char* kDifficulty    = "Складність";
inline constexpr const char* kNoEngine      =
    "Stockfish не знайдено, тому зараз можна грати лише вдвох.";

// The appearance dialog: the sets of pieces and the boards, by name.
inline constexpr const char* kLookPieces    = "Фігури";
inline constexpr const char* kLookBoard     = "Дошка";
inline constexpr const char* kPiecesClassic = "Класичні";
inline constexpr const char* kPiecesGothic  = "Ґотичні";
inline constexpr const char* kBoardWood     = "Дерево";
inline constexpr const char* kBoardGothic   = "Ґотика";
inline constexpr const char* kBoardMarble   = "Мармур";
inline constexpr const char* kBoardEmerald  = "Смарагд";

// Asked before a game that is still being played is thrown away.
inline constexpr const char* kUnfinishedTitle = "Партію не закінчено";
inline constexpr const char* kLoseByNewGame =
    "Якщо почати нову гру, ця партія зникне. Її можна спершу скопіювати — кнопка «Копіювати все».";
inline constexpr const char* kLoseByQuit =
    "Якщо закрити програму, ця партія зникне. Її можна спершу скопіювати — кнопка «Копіювати все».";
inline constexpr const char* kKeepPlaying = "Грати далі";
inline constexpr const char* kQuit        = "Закрити";

// Names beside the board.
inline constexpr const char* kYou      = "Ви";
inline constexpr const char* kComputer = "Stockfish";
inline constexpr const char* kMoves    = "Ходи";
inline constexpr const char* kNoMoves  = "Партія ще не почалася";

// Status lines.
inline constexpr const char* kYourMove      = "Ваш хід";
inline constexpr const char* kWhiteToMove   = "Хід білих";
inline constexpr const char* kBlackToMove   = "Хід чорних";
inline constexpr const char* kThinking      = "Stockfish думає…";
inline constexpr const char* kHintThinking  = "Шукаю найкращий хід…";
inline constexpr const char* kCheck         = "Шах!";
inline constexpr const char* kEngineStarting = "Запускаю Stockfish…";
inline constexpr const char* kEngineLost    = "Рушій не відповідає";
inline constexpr const char* kEngineLostHelp =
    "Почніть нову гру — Stockfish буде запущено знову.";
inline constexpr const char* kTwoPlayers    = "Гра вдвох";

// "Початківець", "Любитель", ... for level 1..8.
std::string levelName(int number);

// One line on what to expect from a level. `elo` is the rating the engine will
// actually be asked for, after clamping to what it supports.
std::string levelDescription(const chess::Level& level, int elo);

// "Рівень 3 · Любитель"
std::string levelLabel(int number);

// Standard notation with the piece letters Ukrainian players write: Кр, Ф, Т,
// С, К, and castling with zeros.
std::string localSan(const std::string& san);

// How the game ended, as a headline and a line of detail. `human` is the side
// the person is playing against the computer, or empty for a two-player game.
struct Verdict {
    std::string headline, detail;
    bool        happy = false;  // the person at the board won
};
Verdict verdict(const chess::Status& status, std::optional<chess::Color> human);

// The score a finished game is written down with — "1–0", "0–1", "½–½" — and
// nothing while it is still being played.
std::string resultSign(const chess::Status& status);

// "+3" for a material lead, empty for none.
std::string lead(int pawns);

}  // namespace text
