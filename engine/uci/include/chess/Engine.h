// A UCI chess engine as the game sees it: ask for a move, get one back later.
//
// The engine is another process and thinks on its own time, so nothing here
// blocks. search() returns at once; a thread of our own reads what the engine
// prints, and poll() hands finished results to whoever asks — in practice the
// game's main thread, once a frame.
//
// Moves cross this boundary as UCI text ("e2e4"). This library does not know
// the rules of chess and does not need to.
#pragma once

#include "chess/Process.h"

#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace chess {

struct SearchRequest {
    // The whole `position ...` command, e.g. "position startpos moves e2e4".
    std::string position;

    // When to stop. At least one should be set; with both, whichever comes
    // first ends the search. With neither, the engine gets one second.
    int moveTimeMs = 0;
    int depth      = 0;

    // Engine options to apply first, as (name, value). Only those that differ
    // from what the engine was last told are actually sent.
    std::vector<std::pair<std::string, std::string>> options;

    // Returned untouched with the result, so the caller can tell which of its
    // questions is being answered.
    int tag = 0;
};

struct SearchResult {
    int         tag = 0;
    std::string bestMove;  // UCI; empty when the engine has no move (mate, stalemate)

    // The engine's last word on the position, from the side to move's point of
    // view: centipawns, or moves to mate (negative: being mated).
    std::optional<int> scoreCp;
    std::optional<int> mateIn;
    int depth = 0;

    // The move the engine currently likes best — the first move of its
    // principal variation. While a search is running this is how to see where
    // it is heading; bestMove is only set once it has finished.
    std::string pvMove;

    // The second-best move and what it is worth, when the engine was asked for
    // two lines (its MultiPV option) and there is a second legal move. Its
    // depth is given separately: the two lines of one iteration arrive one
    // after the other, so for a moment the second is from the iteration before.
    std::string        secondMove;
    std::optional<int> secondCp, secondMate;
    int                secondDepth = 0;
};

class Engine {
public:
    enum class State {
        Stopped,   // never started, or shut down
        Starting,  // process is up, handshake in progress
        Ready,
        Failed,    // could not start, or died; error() says which
    };

    struct Spin {
        int value = 0, min = 0, max = 0;
    };

    Engine() = default;
    ~Engine();

    Engine(const Engine&)            = delete;
    Engine& operator=(const Engine&) = delete;

    // Starts the engine and begins the UCI handshake. Returns false if the
    // program could not be started at all; otherwise the state moves from
    // Starting to Ready (or Failed) on its own. Searches requested before it is
    // ready simply wait.
    bool start(const std::string& path);
    void shutdown();

    State       state() const;
    std::string name() const;   // "Stockfish 17", once known
    std::string error() const;
    std::string path() const;

    // The range of a numeric option the engine declared, e.g. "UCI_Elo".
    std::optional<Spin> spin(const std::string& option) const;
    bool hasOption(const std::string& option) const;

    // Tells the engine the next position belongs to a different game.
    void newGame();

    // Starts thinking. A search already in progress is abandoned: it is told
    // to stop and its answer is thrown away, so exactly one result arrives
    // for the latest request and none for the ones it replaced.
    void search(SearchRequest request);

    // Abandons the current search, if any. No result will be delivered for it.
    void cancel();

    // Is there a request whose result has not been produced yet?
    bool thinking() const;

    // Takes the next finished result, if there is one.
    bool poll(SearchResult& result);

    // What the search in progress thinks so far: its score, depth and
    // preferred move, with bestMove still empty. False if nothing is being
    // searched for anyone, or the engine has not said anything definite yet.
    bool progress(SearchResult& result) const;

    // Every line of the conversation, as it happens: '>' for what we send,
    // '<' for what the engine says. For debugging and for tests. The callback
    // runs on whichever thread the line passed through, with the engine's lock
    // held — it must not call back into this object. Set it before start().
    using Trace = std::function<void(char direction, const std::string& line)>;
    void setTrace(Trace trace);

private:
    void readLoop(Process* process);
    void handleLine(const std::string& line);
    void pump();                        // start the pending search if possible
    void send(const std::string& line); // caller holds _mutex

    mutable std::mutex       _mutex;
    std::unique_ptr<Process> _process;
    std::thread              _reader;

    State       _state = State::Stopped;
    std::string _path, _name, _error;
    Trace       _trace;
    std::map<std::string, Spin>        _spins;
    std::set<std::string>              _known;    // every option the engine declared
    std::map<std::string, std::string> _applied;  // what we last set each option to

    bool _busy     = false;  // a `go` is out and its `bestmove` has not come back
    bool _discard  = false;  // ...and when it does, nobody wants it
    bool _newGame  = false;
    bool _quitting = false;
    std::optional<SearchRequest> _pending;
    SearchResult                 _current;  // being filled in from `info` lines
    std::deque<SearchResult>     _results;
};

// Where to look for the engine, most specific first:
//   1. the CHESS_ENGINE environment variable;
//   2. "stockfish" next to our own executable (inside the .app on a Mac);
//   3. `fallback` — the path the build found it at, if it was given one;
//   4. "stockfish" on the PATH.
// Returns the first that exists and is executable, or an empty string.
std::string findEngine(const std::string& fallback = {});

// The directory this program's executable is in, or empty if unknown.
std::string executableDirectory();

}  // namespace chess
