#include "chess/PositionProbe.h"

#include <utility>

namespace chess {

void PositionProbe::look(const std::string& fen, bool askable) {
    withdraw();
    _fen = fen;
    _delay = -1.0f;
    if (_verdict)
        _changed = true;
    _verdict.reset();
    if (_fen.empty())
        return;

    if (const auto known = _memory.find(_fen); known != _memory.end()) {
        _verdict = known->second;
        _changed = true;
    } else if (askable) {
        _delay = kDelay;
    }
}

void PositionProbe::tick(float seconds) {
    if (_delay < 0.0f)
        return;
    _delay -= seconds;
    if (_delay < 0.0f) {
        _first = true;
        ask(kLongestMate, kFirstMs);
    }
}

PositionProbe::ToEngine PositionProbe::toEngine() {
    ToEngine work{_cancel, std::move(_request)};
    _cancel = false;
    _request.reset();
    return work;
}

void PositionProbe::answer(const SearchResult& result) {
    if (_tag == 0 || result.tag != _tag)
        return;  // an answer about a position that has since been edited
    _tag = 0;

    bool found = false;  // a verdict, or a shorter mate than before
    if (_first) {
        _verdict = Verdict{result.mateIn && *result.mateIn != 0 ? result.mateIn : std::nullopt, result.scoreCp};
        found    = true;
    } else if (result.mateIn && *result.mateIn > 0 && _verdict && _verdict->mate && *result.mateIn < *_verdict->mate) {
        _verdict->mate = result.mateIn;
        found          = true;
    }
    _first   = false;
    _changed = _changed || found;

    // A mate was found; the search stops at the first one it sees, which
    // need not be the quickest. Ask for one a move shorter until there is
    // none — and then the engine has said all it will.
    if (found && _verdict->mate && *_verdict->mate > 1)
        ask(*_verdict->mate - 1, kShorterMs);
    else if (_verdict)
        _memory[_fen] = *_verdict;
}

bool PositionProbe::takeChanged() {
    return std::exchange(_changed, false);
}

void PositionProbe::ask(int mate, int ms) {
    SearchRequest request;
    request.position   = "position fen " + _fen;
    request.mate       = mate;
    request.moveTimeMs = ms;
    request.tag = _tag = _nextTag--;  // negative: a PuzzleSession's are positive
    _request = std::move(request);
}

void PositionProbe::withdraw() {
    if (_tag != 0)
        _cancel = true;
    _tag = 0;
    _request.reset();
    _first = false;
}

}  // namespace chess
