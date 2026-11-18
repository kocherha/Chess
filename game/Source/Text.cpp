#include "Text.h"

#include "Theme.h"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <unordered_map>

using namespace ax;

namespace typeset {

namespace {

// How wide a line of text is when set in `points`. Finding out means drawing
// it — a system-font label is a bitmap the platform renders — and wrapping a
// paragraph asks that once for every word added to a line. The panels lay
// themselves out again on every change, with mostly the same text, so the
// answers are kept: the same question gets the same answer without drawing.
// (The font and the screen's pixel ratio are fixed for the life of the
// program, so an answer never goes stale.)
float widthOf(const std::string& line, float points) {
    static std::unordered_map<std::string, float> known;
    // Hundredths of a point are finer than any size used here.
    std::string key = std::to_string(std::lround(points * 100.0f));
    key += '|';
    key += line;
    if (const auto found = known.find(key); found != known.end())
        return found->second;
    // A bound on what is kept: far more than any screen holds at once.
    if (known.size() >= 4096)
        known.clear();
    const float width = Label::createWithSystemFont(line, theme::kFont, points)->getContentSize().width;
    known.emplace(std::move(key), width);
    return width;
}

}  // namespace

Label* label(const std::string& text, float points, const Color3B& colour) {
    auto* made = Label::createWithSystemFont(text, theme::kFont, points);
    made->setColor(colour);
    return made;
}

Label* leftLabel(const std::string& text, float points, const Color3B& colour) {
    Label* made = label(text, points, colour);
    made->setAnchorPoint(Vec2(0.0f, 0.5f));
    return made;
}

void embolden(Label* label) {
    label->enableShadow(Color4B::WHITE, Vec2(kBoldShift, 0.0f), 0);
}

std::string wrap(const std::string& text, float points, float maxWidth) {
    if (maxWidth <= 0.0f)
        return text;

    std::istringstream words(text);
    std::string out, line;
    for (std::string word; words >> word;) {
        const std::string longer = line.empty() ? word : line + ' ' + word;
        if (line.empty() || widthOf(longer, points) <= maxWidth) {
            line = longer;
        } else {
            out += line + '\n';
            line = word;
        }
    }
    return out + line;
}

std::string wrapParagraphs(const std::string& text, float points, float maxWidth) {
    std::string out;
    std::size_t start = 0;
    while (start <= text.size()) {
        const std::size_t end = std::min(text.find('\n', start), text.size());
        if (!out.empty() || start > 0)
            out += '\n';
        out += wrap(text.substr(start, end - start), points, maxWidth);
        start = end + 1;
    }
    return out;
}

}  // namespace typeset
