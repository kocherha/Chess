// Everything the game says, in one file.
//
// The interface is in Ukrainian. Nothing else in the game contains a word a
// player reads, so putting it into another language means translating this
// file and Strings.cpp, and nothing more.
#pragma once

#include "chess/Difficulty.h"
#include "chess/Game.h"
#include "chess/Setup.h"

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
inline constexpr const char* kPiecesDiagram = "Діаграмні";
inline constexpr const char* kPiecesGothic  = "Ґотичні";
inline constexpr const char* kPiecesAnime   = "Аніме";
inline constexpr const char* kPiecesBone    = "Кістяні";
inline constexpr const char* kPiecesAncient = "Старовинні";
inline constexpr const char* kPiecesPixel   = "Піксельні";
inline constexpr const char* kBoardWood     = "Дерево";
inline constexpr const char* kBoardGothic   = "Ґотика";
inline constexpr const char* kBoardMarble   = "Мармур";
inline constexpr const char* kBoardEmerald  = "Смарагд";
inline constexpr const char* kBoardSakura   = "Сакура";
inline constexpr const char* kBoardLeather  = "Шкіра";
inline constexpr const char* kBoardStone    = "Камінь";
inline constexpr const char* kBoardPixel    = "Пікселі";

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

// The two modes of the game, on the switch under the title: playing a game,
// and setting up positions and solving problems.
inline constexpr const char* kModeGame    = "Партія";
inline constexpr const char* kModePuzzles = "Задачі";

// The position editor.
inline constexpr const char* kEditorTitle   = "Редактор позиції";
inline constexpr const char* kCollection    = "Збірка задач";
inline constexpr const char* kToMove        = "Хід";
inline constexpr const char* kCastling      = "Рокірування";
inline constexpr const char* kCastleShort   = "0-0";
inline constexpr const char* kCastleLong    = "0-0-0";
inline constexpr const char* kEnPassant     = "На проході";
inline constexpr const char* kNoEnPassant   = "—";
inline constexpr const char* kStartPosition = "Початкова";
inline constexpr const char* kClearBoard    = "Очистити";
inline constexpr const char* kCopyFen       = "Копіювати FEN";
inline constexpr const char* kPasteFen      = "Вставити FEN";
inline constexpr const char* kSolve         = "Розв'язувати";
inline constexpr const char* kPlayFrom      = "Грати з позиції";
inline constexpr const char* kSaveMine      = "Зберегти в мої";
inline constexpr const char* kSaved         = "Збережено";
inline constexpr const char* kPasted        = "Вставлено";
inline constexpr const char* kNoFen         = "У буфері немає позиції";
inline constexpr const char* kEditorHelp =
    "Перетягніть фігуру з ряду під дошкою чи над нею, або виберіть її клацанням і ставте на поля. "
    "Фігуру з дошки можна перетягти деінде чи за дошку; праве клацання прибирає.";

inline constexpr const char* kPossible     = "Позиція можлива";
inline constexpr const char* kImpossible   = "Такої позиції не буває";
inline constexpr const char* kUnfinished   = "Позицію ще не закінчено";
inline constexpr const char* kStudying     = "Stockfish вивчає позицію…";
inline constexpr const char* kNoEngineHere = "Stockfish не знайдено: розв'язки перевірити нема чим.";

// Solving.
inline constexpr const char* kChecking      = "Перевіряю хід…";
inline constexpr const char* kDefending     = "Суперник шукає захист…";
inline constexpr const char* kRight         = "Так!";
inline constexpr const char* kWrong         = "Ні, не те";
inline constexpr const char* kSolved        = "Розв'язано!";
inline constexpr const char* kSolution      = "Розв'язок";
inline constexpr const char* kShowingSolution = "Показую розв'язок";
inline constexpr const char* kRestart       = "Спочатку";
inline constexpr const char* kNextPuzzle    = "Наступна задача";
inline constexpr const char* kEditPosition  = "Змінити позицію";
inline constexpr const char* kDeleteMine    = "Видалити з моїх";
inline constexpr const char* kMine          = "Мої задачі";
inline constexpr const char* kMineEmpty =
    "Тут з'являться позиції, збережені в редакторі кнопкою «Зберегти в мої».";
inline constexpr const char* kTheirTurn     = "Хід суперника";
inline constexpr const char* kTryAgain      = "Спробуйте інший хід.";
inline constexpr const char* kBestMoveGoal  = "знайти найкращий хід";

inline constexpr const char* kFromEditor   = "Позиція з редактора";
inline constexpr const char* kNoForcedMate =
    "Форсованого мату Stockfish тут не бачить, тож задача буде на найкращий хід.";
inline constexpr const char* kSolvedAlone  = "Мат поставлено. Задачу розв'язано.";
inline constexpr const char* kSolvedHelped = "Мат поставлено — з підказкою.";
inline constexpr const char* kSolutionDone = "Мат. Спробуйте розв'язати самостійно — «Спочатку».";
inline constexpr const char* kFindingBest  = "Stockfish шукає найкращий хід…";
inline constexpr const char* kHintLooking  = "Шукаю підказку…";

// The set-up position is legal but the game in it is already over.
std::string alreadyOver(chess::Color toMove, bool inCheck);

// "Хід білих" / "Хід чорних" for the line under the board.
const char* toMoveCaption(chess::Color color);

// Why a set-up position is not possible, in one sentence.
std::string issue(const chess::Issue& issue);

// "Білі починають і ставлять мат у 2 ходи." — or, with mateIn 0, "...і
// знаходять найкращий хід".
std::string goal(chess::Color first, int mateIn);

// "мат у 2 ходи", "мат в 1 хід", "мат у 5 ходів".
std::string mateIn(int moves);

// What a search found about a set-up position: "Білі ставлять мат у 3
// ходи" or "Оцінка: +1,4 (у білих краще)".
std::string finding(chess::Color toMove, std::optional<int> mate, std::optional<int> cp);

// "Моя задача 3"
std::string minePuzzleTitle(int number);

// "Почати задачу знову?" and the like are not needed: puzzles cost nothing.

// "Білі", "Чорні" for a side as the subject of a sentence.
const char* side(chess::Color color);

// "Тб7 правильно. Тепер чорні: …" — the reply after a right move.
std::string rightMove(const std::string& move);
// "Після Кf3 мату в 2 немає." — after a wrong one.
std::string wrongMove(const std::string& move, int movesLeft);
// The best-move puzzle: what the move was worth next to the best.
std::string bestMoveVerdict(const std::string& move, bool right, const std::string& best);

// "Початківець", "Любитель", ... for level 1..8.
std::string levelName(int number);

// The opponent's strength as a number: "≈ 1750 Elo", "23 % ходів — випадкові",
// "повна сила". `elo` here and below is the rating the engine will actually be
// asked for, after clamping to what it supports.
std::string strengthMeasure(const chess::Level& level, int elo);

// One line on what to expect from the opponent: of a level, or of anywhere
// between that level and the next.
std::string levelDescription(const chess::Level& level, int elo);

// The line on the panel during a game: "Рівень 3 · Любитель" for a level,
// "Досвідчений · ≈ 1750 Elo" for a strength between two.
std::string strengthLabel(const chess::Level& level, int elo);

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
