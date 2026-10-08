#include "Text.h"

#include "Theme.h"

#include <sstream>

using namespace ax;

namespace typeset {

Label* label(const std::string& text, float points, const Color3B& colour) {
    auto* made = Label::createWithSystemFont(text, theme::kFont, points);
    made->setColor(colour);
    return made;
}

std::string wrap(const std::string& text, float points, float maxWidth) {
    if (maxWidth <= 0.0f)
        return text;

    const auto widthOf = [points](const std::string& line) {
        return Label::createWithSystemFont(line, theme::kFont, points)->getContentSize().width;
    };

    std::istringstream words(text);
    std::string out, line;
    for (std::string word; words >> word;) {
        const std::string longer = line.empty() ? word : line + ' ' + word;
        if (line.empty() || widthOf(longer) <= maxWidth) {
            line = longer;
        } else {
            out += line + '\n';
            line = word;
        }
    }
    return out + line;
}

}  // namespace typeset
