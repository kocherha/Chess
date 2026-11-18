#include "Strings.h"

#include <cmath>
#include <cstdlib>

namespace text {

using chess::Color;
using chess::Outcome;

std::string levelName(int number) {
    switch (number) {
    case 1:  return "Початківець";
    case 2:  return "Новачок";
    case 3:  return "Любитель";
    case 4:  return "Досвідчений";
    case 5:  return "Клубний гравець";
    case 6:  return "Кандидат у майстри";
    case 7:  return "Гросмейстер";
    default: return "Без обмежень";
    }
}

namespace {

// "23 %": the share of the opponent's moves that are not the engine's.
std::string percent(double share) {
    return std::to_string(std::lround(share * 100.0)) + " %";
}

}  // namespace

std::string strengthMeasure(const chess::Level& level, int elo) {
    if (level.fullStrength())
        return "повна сила";
    if (level.randomMoveShare > 0.0)
        return percent(level.randomMoveShare) + " ходів — випадкові";
    return "≈ " + std::to_string(elo) + " Elo";
}

std::string levelDescription(const chess::Level& level, int elo) {
    if (level.fullStrength())
        return "Повна сила рушія. Найсильніші шахісти світу йому програють.";

    // Each of these has to be true all the way from its level to the next:
    // the slider stops anywhere between them.
    switch (level.number) {
    case 1:
        return "Часто робить випадкові ходи й зіває фігури. Для перших партій.";
    case 2:
        return "Грає уважніше, але ще помиляється на рівному місці.";
    case 3:
        return "Уже без випадкових ходів: слабка, але «чесна» гра Stockfish.";
    case 4:
        return "Сильний аматор: помилок не прощає, але й сам їх робить.";
    case 5:
        return "Тут уже доводиться рахувати варіанти на кілька ходів.";
    case 6:
        return "Перемога над ним — серйозне досягнення.";
    default:
        // The highest rating a person has ever had is a little under 2900.
        return elo < 2900 ? "Так грають найсильніші турнірні шахісти."
                          : "Такого рейтингу не мав іще жоден шахіст.";
    }
}

std::string strengthLabel(const chess::Level& level, int elo) {
    if (level.exact)
        return "Рівень " + std::to_string(level.number) + " · " + levelName(level.number);
    // Short enough for the panel: the share of random moves without the dash.
    const std::string measure = level.randomMoveShare > 0.0 ? percent(level.randomMoveShare) + " випадкових ходів"
                                                            : strengthMeasure(level, elo);
    return levelName(level.number) + " · " + measure;
}

std::string localSan(const std::string& san) {
    if (san.rfind("O-O-O", 0) == 0)
        return "0-0-0" + san.substr(5);
    if (san.rfind("O-O", 0) == 0)
        return "0-0" + san.substr(3);

    std::string out;
    for (char c : san) {
        switch (c) {
        case 'K': out += "Кр"; break;
        case 'Q': out += "Ф"; break;
        case 'R': out += "Т"; break;
        case 'B': out += "С"; break;
        case 'N': out += "К"; break;
        default:  out += c; break;
        }
    }
    return out;
}

Verdict verdict(const chess::Status& status, std::optional<Color> human) {
    Verdict result;

    const auto winnerName = [&](Color winner) -> std::string {
        if (!human)
            return winner == Color::White ? "Перемогли білі" : "Перемогли чорні";
        return winner == *human ? "Ви перемогли!" : "Переміг Stockfish";
    };

    switch (status.outcome) {
    case Outcome::Ongoing:
        break;
    case Outcome::Checkmate:
        result.headline = "Мат";
        result.detail   = winnerName(*status.winner);
        result.happy    = human && *status.winner == *human;
        break;
    case Outcome::Resignation:
        if (human && *status.winner != *human) {
            result.headline = "Ви здалися";
            result.detail   = "Переміг Stockfish";
        } else {
            result.headline = *status.winner == Color::White ? "Чорні здалися" : "Білі здалися";
            result.detail   = winnerName(*status.winner);
        }
        break;
    case Outcome::Stalemate:
        result.headline = "Пат";
        result.detail   = "Нічия: ходів немає, а шаху теж";
        break;
    case Outcome::Repetition:
        result.headline = "Нічия";
        result.detail   = "Позиція повторилася тричі";
        break;
    case Outcome::FiftyMoves:
        result.headline = "Нічия";
        result.detail   = "50 ходів без взяття й руху пішака";
        break;
    case Outcome::InsufficientMaterial:
        result.headline = "Нічия";
        result.detail   = "Матеріалу замало, щоб поставити мат";
        break;
    }
    return result;
}

std::string resultSign(const chess::Status& status) {
    if (!status.over())
        return {};
    if (!status.winner)
        return "½–½";
    return *status.winner == Color::White ? "1–0" : "0–1";
}

std::string lead(int pawns) {
    return pawns > 0 ? "+" + std::to_string(pawns) : std::string();
}

// --- positions and problems ----------------------------------------------------------

namespace {

// The form of a noun after a number: 1 хід, 2 ходи, 5 ходів; 11–14 take the
// third form whatever their last digit.
const char* plural(int n, const char* one, const char* few, const char* many) {
    const int tens = (n / 10) % 10, last = n % 10;
    if (tens != 1 && last == 1)
        return one;
    if (tens != 1 && last >= 2 && last <= 4)
        return few;
    return many;
}

std::string count(int n, const char* one, const char* few, const char* many) {
    return std::to_string(n) + ' ' + plural(n, one, few, many);
}

// "a8, h8"
std::string squares(const std::vector<chess::Square>& list) {
    std::string out;
    for (const chess::Square square : list) {
        if (!out.empty())
            out += ", ";
        out += chess::squareName(square);
    }
    return out;
}

// "білий", "білого" and the rest: the adjective for a side, in the case asked for.
const char* whiteOrBlack(Color color, const char* white, const char* black) {
    return color == Color::White ? white : black;
}

}  // namespace

const char* side(Color color) {
    return color == Color::White ? "Білі" : "Чорні";
}

std::string issue(const chess::Issue& issue) {
    using chess::Flaw;
    const Color c = issue.color;
    const std::string where = issue.squares.empty() ? std::string() : " (" + squares(issue.squares) + ")";

    switch (issue.flaw) {
    case Flaw::MissingKing:
        return std::string("Немає ") + whiteOrBlack(c, "білого", "чорного") + " короля.";
    case Flaw::ExtraKing:
        return std::string("Більше одного ") + whiteOrBlack(c, "білого", "чорного") + " короля" + where + ".";
    case Flaw::PawnOnEdge:
        if (issue.squares.size() > 1)
            return std::string(whiteOrBlack(c, "Білі пішаки", "Чорні пішаки")) + where +
                   " на крайніх горизонталях: на першій пішаків не буває, а на останній пішак стає фігурою.";
        return std::string(whiteOrBlack(c, "Білий пішак", "Чорний пішак")) + where +
               " на крайній горизонталі: на першій пішаків не буває, а на останній пішак стає фігурою.";
    case Flaw::KingsTouching:
        return "Королі стоять поруч" + where + ": так вони били б один одного.";
    case Flaw::WaitingSideInCheck:
        return std::string(side(c)) + " під шахом" + where + ", а хід " +
               whiteOrBlack(c, "чорних", "білих") + ": щойно ходили " + whiteOrBlack(c, "білі", "чорні") +
               ", і не могли лишити свого короля під боєм.";
    case Flaw::TooManyCheckers:
        return std::string("Шах ") + whiteOrBlack(c, "білому", "чорному") +
               " королю від трьох фігур" + where + ": один хід дає щонайбільше подвійний шах.";
    case Flaw::ImpossibleCheck:
        return std::string("Такий шах ") + whiteOrBlack(c, "білому", "чорному") + " королю" + where +
               " не міг дати жоден хід: пішак чи кінь не відкривають шаху, а пішак на своєму першому полі ще не ходив.";
    case Flaw::TooManyPawns:
        return std::string("У ") + whiteOrBlack(c, "білих ", "чорних ") +
               count(issue.need, "пішак", "пішаки", "пішаків") + " — буває щонайбільше 8.";
    case Flaw::TooManyPieces:
        return std::string("У ") + whiteOrBlack(c, "білих", "чорних") + " забагато фігур: для цього треба " +
               count(issue.need, "перетворення", "перетворення", "перетворень") + " пішака, а пішаків бракує " +
               (issue.have == 0 ? std::string("жодного") : "лише " + std::to_string(issue.have)) + ".";
    case Flaw::PawnCaptures:
        if (issue.need < 0)
            return std::string(whiteOrBlack(c, "Білі", "Чорні")) + " пішаки" + where +
                   " не могли так стати: кожен іде своєю вертикаллю і зміщується лише взяттям.";
        return std::string(whiteOrBlack(c, "Білі", "Чорні")) + " пішаки" + where + " стоять так лише після " +
               count(issue.need, "взяття", "взять", "взять") + ", а " + whiteOrBlack(c, "чорних", "білих") +
               " фігур бракує " + (issue.have == 0 ? std::string("жодної") : "лише " + std::to_string(issue.have)) +
               ".";
    case Flaw::BadEnPassant:
        return "Взяття на проході" + where +
               " не узгоджується з шахом: щойно зроблений хід пішака на два поля не міг його дати.";
    }
    return {};
}

std::string mateIn(int moves) {
    // "в" before a vowel — один, одинадцять — "у" otherwise.
    const bool vowel = moves == 1 || moves == 11;
    return std::string("мат ") + (vowel ? "в " : "у ") + count(moves, "хід", "ходи", "ходів");
}

std::string goal(Color first, int moves) {
    if (moves <= 0)
        return std::string(side(first)) + " починають і знаходять найкращий хід.";
    return std::string(side(first)) + " починають і ставлять " + mateIn(moves) + ".";
}

std::string finding(Color toMove, std::optional<int> mate, std::optional<int> cp) {
    if (mate && *mate > 0)
        return std::string(side(toMove)) + " ставлять " + mateIn(*mate) + ".";
    if (mate && *mate < 0)
        return std::string(side(chess::opposite(toMove))) + " ставлять " + mateIn(-*mate) + ".";
    if (!cp)
        return {};
    // For white, in pawns, with a decimal comma.
    const int white = toMove == Color::White ? *cp : -*cp;
    const int tenths = (std::abs(white) + 5) / 10;
    std::string value = (white > 0 ? "+" : white < 0 ? "−" : "") + std::to_string(tenths / 10) + "," +
                        std::to_string(tenths % 10);
    const char* who = tenths <= 3 ? "приблизно рівно" : white > 0 ? "у білих краще" : "у чорних краще";
    return "Оцінка Stockfish: " + value + " (" + who + ").";
}

std::string alreadyOver(Color toMove, bool inCheck) {
    if (inCheck)
        return std::string("Мат: ") + whiteOrBlack(toMove, "білим", "чорним") +
               " вже поставлено мат, ходити нікому.";
    return std::string("Пат: у ") + whiteOrBlack(toMove, "білих", "чорних") + " немає ходів, і шаху немає.";
}

const char* toMoveCaption(Color color) {
    return color == Color::White ? kWhiteToMove : kBlackToMove;
}

PuzzleName puzzleName(std::string_view id) {
    static const struct {
        const char* id;
        PuzzleName  name;
    } names[] = {
        {"m1-back-rank", {"Остання горизонталь", ""}},
        {"m1-black-back-rank", {"Остання горизонталь, чорні", ""}},
        {"m1-smothered", {"Спертий мат", ""}},
        {"m1-arabian", {"Арабський мат", ""}},
        {"m1-anastasia", {"Мат Анастасії", ""}},
        {"m1-boden", {"Мат Бодена", ""}},
        {"m1-epaulette", {"Еполетний мат", ""}},
        {"m1-scholar", {"Дитячий мат", ""}},
        {"m1-fool", {"Дурний мат", ""}},
        {"m2-morphy", {"Задача Морфі", "Пол Морфі"}},
        {"m2-opera", {"Партія в опері", "Морфі — герцог Брауншвейзький і граф Ізуар, Париж, 1858"}},
        {"m2-legal", {"Мат Легаля", ""}},
        {"m2-smothered", {"Спертий мат із жертвою", ""}},
        {"m2-anastasia", {"Мат Анастасії з жертвою", ""}},
        {"m2-king-rook", {"Король і тура", ""}},
        {"m2-two-rooks", {"Дві тури", ""}},
        {"m2-black-rooks", {"Чорні тури", ""}},
        {"m3-queen-rook", {"Ферзь і тура", ""}},
        {"m3-rook-bishop", {"Тура й слон", ""}},
        {"m3-black-queen", {"Чорний ферзь", ""}},
        {"m3-immortal", {"Безсмертна партія", "Андерсен — Кізерицький, Лондон, 1851"}},
        {"m4-philidor", {"Мат Філідора", ""}},
        {"m5-bishop-rook-queen", {"Слон, тура й ферзь", ""}},
    };
    for (const auto& entry : names)
        if (id == entry.id)
            return entry.name;
    return {};
}

std::string minePuzzleTitle(int number) {
    return "Моя задача " + std::to_string(number);
}

std::string rightMove(const std::string& move) {
    return move + " — правильно.";
}

std::string wrongMove(const std::string& move, int movesLeft) {
    const bool vowel = movesLeft == 1 || movesLeft == 11;
    return "Після " + move + " мату " + (vowel ? "в " : "у ") + count(movesLeft, "хід", "ходи", "ходів") +
           " немає.";
}

std::string bestMoveVerdict(const std::string& move, bool right, const std::string& best) {
    if (right)
        return best.empty() || best == move ? move + " — найкращий хід." : move + " — не гірше за найкращий (" + best + ").";
    return move + " помітно слабше за найкращий хід.";
}

}  // namespace text
