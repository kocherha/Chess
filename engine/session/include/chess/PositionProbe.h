// What the engine finds in a position set up in the editor: a forced mate —
// the shortest there is — or who is better.
//
// A position is asked about a moment after the last edit, not at every piece
// put down on the way to it. The first search looks for a mate in up to
// kLongestMate moves; it stops at the first one it sees, which need not be
// the quickest, so while it has found one it is asked again for one a move
// shorter, until there is none. What it said is remembered by position (the
// FEN), in a memory the caller owns and can keep for longer than this
// object: an edit undone, or the editor opened again, asks nothing twice.
//
// Like the other models here it decides what to ask and does not ask it:
// the scene calls look() when the position changes, tick() with the time,
// does with the engine what toEngine() says and passes its answers to
// answer(). Its tags are negative, so that one engine can also serve a
// PuzzleSession, whose tags are positive.
//
// In the tests without an engine: engine/session/tests/PositionProbeTest.cpp.
#pragma once

#include "chess/Engine.h"

#include <map>
#include <optional>
#include <string>

namespace chess {

class PositionProbe {
public:
    static constexpr float kDelay       = 0.3f;  // seconds after the last edit, before the engine is asked
    static constexpr int   kLongestMate = 8;     // the longest mate looked for
    static constexpr int   kFirstMs     = 3000;
    static constexpr int   kShorterMs   = 2000;  // then: is there a shorter one?

    // The shortest mate found (negative: the side to move is mated), or none,
    // and the score otherwise.
    struct Verdict {
        std::optional<int> mate, cp;

        bool operator==(const Verdict&) const = default;
    };
    using Memory = std::map<std::string, Verdict>;  // by FEN

    explicit PositionProbe(Memory& memory) : _memory(memory) {}

    // The position in the editor is now this one. `askable`: it is worth
    // asking about — possible, with a move to make, and there is an engine.
    // Whatever was being asked about the last one is withdrawn.
    void look(const std::string& fen, bool askable);
    // Nothing is being edited any more.
    void drop() { look({}, false); }

    void tick(float seconds);

    struct ToEngine {
        bool                         cancel = false;
        std::optional<SearchRequest> search;
    };
    ToEngine toEngine();
    void     answer(const SearchResult& result);

    // The wait after an edit is running: the screen has to keep drawing, or
    // the question would never be asked.
    bool waiting() const { return _delay >= 0.0f; }

    // What the engine has said about the position: nothing yet, or its
    // verdict — which may still get a shorter mate while it goes on looking.
    const std::optional<Verdict>& verdict() const { return _verdict; }
    // The mate to set as a problem's goal: the moves to mate if the side to
    // move mates, else 0 (a best-move problem).
    int mateToSolve() const { return _verdict && _verdict->mate && *_verdict->mate > 0 ? *_verdict->mate : 0; }

    // Has verdict() changed since this was last asked?
    bool takeChanged();

private:
    void ask(int mate, int ms);
    void withdraw();

    Memory&     _memory;
    std::string _fen;            // the position looked at; empty for none
    float       _delay = -1.0f;  // seconds until it is asked about; < 0: not due
    std::optional<Verdict> _verdict;
    bool                   _changed = false;

    int                          _tag = 0, _nextTag = -1;
    std::optional<SearchRequest> _request;
    bool                         _cancel = false;
    bool                         _first  = false;  // the question out is the first one about this position
};

}  // namespace chess
