#include "Strings.h"

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

std::string levelDescription(const chess::Level& level, int elo) {
    if (level.fullStrength())
        return "Повна сила рушія. Найсильніші шахісти світу йому програють.";

    const std::string rating = "≈ " + std::to_string(elo) + " Elo";
    switch (level.number) {
    case 1:
        return "Часто робить випадкові ходи й зіває фігури. Для перших партій.";
    case 2:
        return "Грає уважніше, але ще помиляється на рівному місці.";
    case 3:
        return rating + ". Найслабша «чесна» гра Stockfish: без випадкових ходів.";
    case 4:
        return rating + ". Сильний аматор: помилок не прощає, але й сам їх робить.";
    case 5:
        return rating + ". Тут уже доводиться рахувати варіанти на кілька ходів.";
    case 6:
        return rating + ". Перемога над ним — серйозне досягнення.";
    default:
        return rating + ". Так грають найсильніші турнірні шахісти.";
    }
}

std::string levelLabel(int number) {
    return "Рівень " + std::to_string(number) + " · " + levelName(number);
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

std::string lead(int pawns) {
    return pawns > 0 ? "+" + std::to_string(pawns) : std::string();
}

}  // namespace text
