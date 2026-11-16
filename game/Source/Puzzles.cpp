#include "Puzzles.h"

#include "Strings.h"

#include "axmol.h"

#include <algorithm>
#include <sstream>

using namespace ax;

namespace puzzles {

namespace {

// The collection. Checked with tools outside the game (Stockfish, `go mate N`
// and then each first move on its own with `searchmoves`): every one is a mate
// in exactly the number of moves given, and every one but "Дві тури" has a
// single first move that does it.
const std::vector<Puzzle> kCollection = {
    // Mate in one: the patterns every player should know by sight.
    {"m1-back-rank", "Остання горизонталь", "", "6k1/5ppp/8/8/8/8/5PPP/4R1K1 w - - 0 1", 1},
    {"m1-black-back-rank", "Остання горизонталь, чорні", "", "4r1k1/5ppp/8/8/8/8/5PPP/6K1 b - - 0 1", 1},
    {"m1-smothered", "Спертий мат", "", "6rk/6pp/7N/8/8/8/8/6K1 w - - 0 1", 1},
    {"m1-arabian", "Арабський мат", "", "7k/R7/5N2/8/8/8/8/6K1 w - - 0 1", 1},
    {"m1-anastasia", "Мат Анастасії", "", "5r2/4Nppk/8/R7/8/8/8/6K1 w - - 0 1", 1},
    {"m1-boden", "Мат Бодена", "", "2kr4/3p4/8/8/5B2/8/8/5BK1 w - - 0 1", 1},
    {"m1-epaulette", "Еполетний мат", "", "3rkr2/8/8/8/8/8/Q7/6K1 w - - 0 1", 1},
    {"m1-scholar", "Дитячий мат", "", "r1bqkb1r/pppp1ppp/2n2n2/4p2Q/2B1P3/8/PPPP1PPP/RNB1K1NR w KQkq - 4 4", 1},
    {"m1-fool", "Дурний мат", "", "rnbqkbnr/pppp1ppp/8/4p3/6P1/5P2/PPPPP2P/RNBQKBNR b KQkq - 0 2", 1},

    // Mate in two.
    {"m2-morphy", "Задача Морфі", "Пол Морфі", "kbK5/pp6/1P6/8/8/8/8/R7 w - - 0 1", 2},
    {"m2-opera", "Партія в опері", "Морфі — герцог Брауншвейзький і граф Ізуар, Париж, 1858",
     "4kb1r/p2n1ppp/4q3/4p1B1/4P3/1Q6/PPP2PPP/2KR4 w k - 1 16", 2},
    {"m2-legal", "Мат Легаля", "", "rn1qkbnr/ppp2p1p/3p2p1/4N3/2B1P3/2N5/PPPP1PPP/R1BbK2R w KQkq - 0 6", 2},
    {"m2-smothered", "Спертий мат із жертвою", "", "5r1k/6pp/7N/8/2Q5/8/8/6K1 w - - 0 1", 2},
    {"m2-anastasia", "Мат Анастасії з жертвою", "", "5r1k/4Nppp/8/7Q/8/3R4/8/6K1 w - - 0 1", 2},
    {"m2-king-rook", "Король і тура", "", "k7/8/8/1K6/8/8/8/7R w - - 0 1", 2},
    {"m2-two-rooks", "Дві тури", "", "7k/8/8/8/8/8/R7/1R4K1 w - - 0 1", 2},
    {"m2-black-rooks", "Чорні тури", "", "6k1/pp4p1/2p5/2bp4/8/P5Pb/1P3rrP/2BRRN1K b - - 0 1", 2},

    // Mate in three and more.
    {"m3-queen-rook", "Ферзь і тура", "", "6k1/5p1p/6p1/8/8/8/5PPP/2Q1R1K1 w - - 0 1", 3},
    {"m3-rook-bishop", "Тура й слон", "", "r5rk/5p1p/5R2/4B3/8/8/7P/7K w - - 0 1", 3},
    {"m3-black-queen", "Чорний ферзь", "", "2r3k1/p4p2/3Rp2p/1p2P1pK/8/1P4P1/P3Q2P/1q6 b - - 0 1", 3},
    {"m3-immortal", "Безсмертна партія", "Андерсен — Кізерицький, Лондон, 1851",
     "r1b1k1nr/p2p1ppp/n2B4/1p1NPN1P/6P1/3P1Q2/P1P1K3/q5b1 w kq - 0 1", 3},
    {"m4-philidor", "Мат Філідора", "", "r6k/6pp/8/4N3/2Q5/8/5PPP/6K1 w - - 0 1", 4},
    {"m5-bishop-rook-queen", "Слон, тура й ферзь", "", "5r1k/6pp/8/8/8/1B6/6PP/3R2QK w - - 0 1", 5},
};

constexpr const char* kMineKey   = "puzzles.mine";      // one per line: number, mate, FEN
constexpr const char* kSolvedKey = "puzzles.solved";    // ids, one per line

std::vector<std::string> lines(const std::string& text) {
    std::vector<std::string> out;
    std::istringstream in(text);
    for (std::string line; std::getline(in, line);)
        if (!line.empty())
            out.push_back(line);
    return out;
}

std::string joined(const std::vector<std::string>& list) {
    std::string out;
    for (const std::string& line : list)
        out += line + '\n';
    return out;
}

void store(const char* key, const std::vector<std::string>& list) {
    auto* defaults = UserDefault::getInstance();
    defaults->setStringForKey(key, joined(list));
    defaults->flush();
}

std::vector<std::string> load(const char* key) {
    return lines(std::string(UserDefault::getInstance()->getStringForKey(key, "")));
}

// "3|2|<fen>" → the puzzle.
bool parse(const std::string& line, Puzzle& out) {
    const std::size_t a = line.find('|'), b = a == std::string::npos ? a : line.find('|', a + 1);
    if (b == std::string::npos)
        return false;
    const int number = std::atoi(line.substr(0, a).c_str());
    out        = Puzzle{};
    out.id     = "mine-" + std::to_string(number);
    out.title  = text::minePuzzleTitle(number);
    out.mateIn = std::atoi(line.substr(a + 1, b - a - 1).c_str());
    out.fen    = line.substr(b + 1);
    out.mine   = true;
    return number > 0 && !out.fen.empty();
}

}  // namespace

const std::vector<Puzzle>& collection() {
    return kCollection;
}

std::vector<Puzzle> mine() {
    std::vector<Puzzle> out;
    for (const std::string& line : load(kMineKey)) {
        Puzzle puzzle;
        if (parse(line, puzzle))
            out.push_back(puzzle);
    }
    return out;
}

Puzzle save(const std::string& fen, int mateIn) {
    std::vector<std::string> list = load(kMineKey);
    int highest = 0;
    for (const std::string& line : list) {
        Puzzle puzzle;
        if (!parse(line, puzzle))
            continue;
        if (puzzle.fen == fen)
            return puzzle;
        highest = std::max(highest, std::atoi(line.c_str()));
    }
    const std::string line = std::to_string(highest + 1) + '|' + std::to_string(mateIn) + '|' + fen;
    list.push_back(line);
    store(kMineKey, list);
    Puzzle puzzle;
    parse(line, puzzle);
    return puzzle;
}

void remove(const std::string& id) {
    std::vector<std::string> list = load(kMineKey);
    list.erase(std::remove_if(list.begin(), list.end(),
                              [&](const std::string& line) {
                                  Puzzle puzzle;
                                  return parse(line, puzzle) && puzzle.id == id;
                              }),
               list.end());
    store(kMineKey, list);

    std::vector<std::string> done = load(kSolvedKey);
    done.erase(std::remove(done.begin(), done.end(), id), done.end());
    store(kSolvedKey, done);
}

bool solved(const std::string& id) {
    const auto done = load(kSolvedKey);
    return std::find(done.begin(), done.end(), id) != done.end();
}

void markSolved(const std::string& id) {
    if (id.empty() || solved(id))
        return;
    auto done = load(kSolvedKey);
    done.push_back(id);
    store(kSolvedKey, done);
}

std::vector<Puzzle> all() {
    std::vector<Puzzle> out = collection();
    for (const Puzzle& puzzle : mine())
        out.push_back(puzzle);
    return out;
}

Puzzle next(const std::string& id) {
    const auto list = all();
    if (list.empty())
        return {};
    const auto at = std::find_if(list.begin(), list.end(), [&](const Puzzle& p) { return p.id == id; });
    // From the one after this, round to the start: the first not yet solved;
    // if every one is, simply the next.
    const std::size_t start = at == list.end() ? 0 : static_cast<std::size_t>(at - list.begin()) + 1;
    for (std::size_t i = 0; i < list.size(); ++i) {
        const Puzzle& candidate = list[(start + i) % list.size()];
        if (candidate.id != id && !solved(candidate.id))
            return candidate;
    }
    return list[start % list.size()];
}

Puzzle find(const std::string& id) {
    for (const Puzzle& puzzle : all())
        if (puzzle.id == id)
            return puzzle;
    return {};
}

}  // namespace puzzles
