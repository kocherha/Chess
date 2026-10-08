#include "chess/Engine.h"

#include <cstdint>
#include <cstdlib>
#include <sstream>

#if defined(__APPLE__)
#    include <mach-o/dyld.h>
#    include <climits>
#    include <unistd.h>
#elif !defined(_WIN32)
#    include <climits>
#    include <unistd.h>
#endif

namespace chess {

namespace {

std::vector<std::string> words(const std::string& line) {
    std::vector<std::string> out;
    std::istringstream in(line);
    for (std::string word; in >> word;)
        out.push_back(std::move(word));
    return out;
}

bool toInt(const std::string& text, int& out) {
    char* end        = nullptr;
    const long value = std::strtol(text.c_str(), &end, 10);
    if (end == text.c_str() || *end != '\0')
        return false;
    out = static_cast<int>(value);
    return true;
}

}  // namespace

Engine::~Engine() {
    shutdown();
}

bool Engine::start(const std::string& path) {
    shutdown();

    std::lock_guard lock(_mutex);
    _path = path;
    _name.clear();
    _error.clear();
    _spins.clear();
    _known.clear();
    _applied.clear();
    _results.clear();
    _pending.reset();
    _busy = _discard = _newGame = _quitting = false;

    auto process = std::make_unique<Process>();
    std::string why;
    if (!process->start(path, &why)) {
        _state = State::Failed;
        _error = why;
        return false;
    }

    _process = std::move(process);
    _state   = State::Starting;
    send("uci");
    _reader = std::thread([this, process = _process.get()] { readLoop(process); });
    return true;
}

void Engine::shutdown() {
    Process* process = nullptr;
    {
        std::lock_guard lock(_mutex);
        _pending.reset();
        if (!_process) {
            if (_state != State::Failed)
                _state = State::Stopped;
            return;
        }
        _quitting = true;
        // `stop` first: an engine deep in a search does not look at its input
        // for anything else until the search is over.
        if (_busy)
            send("stop");
        send("quit");
        // Under the lock, so that the reader thread is never caught half-way
        // through a write of its own.
        _process->closeInput();
        process = _process.get();
    }

    // Outside the lock: the reader takes it for every line it handles, and it
    // has to be able to run to the end of the engine's output.
    process->stop();
    if (_reader.joinable())
        _reader.join();

    std::lock_guard lock(_mutex);
    _process.reset();
    _state = State::Stopped;
    _busy = _discard = _quitting = false;
}

Engine::State Engine::state() const {
    std::lock_guard lock(_mutex);
    return _state;
}

std::string Engine::name() const {
    std::lock_guard lock(_mutex);
    return _name;
}

std::string Engine::error() const {
    std::lock_guard lock(_mutex);
    return _error;
}

std::string Engine::path() const {
    std::lock_guard lock(_mutex);
    return _path;
}

std::optional<Engine::Spin> Engine::spin(const std::string& option) const {
    std::lock_guard lock(_mutex);
    const auto found = _spins.find(option);
    if (found == _spins.end())
        return std::nullopt;
    return found->second;
}

bool Engine::hasOption(const std::string& option) const {
    std::lock_guard lock(_mutex);
    return _known.count(option) != 0;
}

void Engine::newGame() {
    std::lock_guard lock(_mutex);
    _newGame = true;
}

void Engine::search(SearchRequest request) {
    std::lock_guard lock(_mutex);
    if (_state == State::Failed || _state == State::Stopped)
        return;

    _pending = std::move(request);
    if (_busy && !_discard) {
        _discard = true;
        send("stop");
    }
    pump();
}

void Engine::cancel() {
    std::lock_guard lock(_mutex);
    _pending.reset();
    if (_busy && !_discard) {
        _discard = true;
        send("stop");
    }
}

bool Engine::thinking() const {
    std::lock_guard lock(_mutex);
    if (_state == State::Failed || _state == State::Stopped)
        return false;
    return _pending.has_value() || (_busy && !_discard);
}

bool Engine::progress(SearchResult& result) const {
    std::lock_guard lock(_mutex);
    if (!_busy || _discard || (!_current.scoreCp && !_current.mateIn))
        return false;
    result = _current;
    return true;
}

bool Engine::poll(SearchResult& result) {
    std::lock_guard lock(_mutex);
    if (_results.empty())
        return false;
    result = std::move(_results.front());
    _results.pop_front();
    return true;
}

void Engine::setTrace(Trace trace) {
    std::lock_guard lock(_mutex);
    _trace = std::move(trace);
}

void Engine::send(const std::string& line) {
    if (!_process)
        return;
    if (_trace)
        _trace('>', line);
    if (!_process->writeLine(line) && !_quitting && _state != State::Failed) {
        _state = State::Failed;
        _error = "the engine stopped responding";
    }
}

void Engine::pump() {
    // UCI is strictly one search at a time: a new `position` may only follow
    // the `bestmove` of the previous `go`. So a request made while the engine
    // is still busy waits here until handleLine() sees that bestmove.
    if (_state != State::Ready || _busy || !_pending)
        return;

    SearchRequest request = std::move(*_pending);
    _pending.reset();

    if (_newGame) {
        send("ucinewgame");
        _newGame = false;
    }

    for (const auto& [option, value] : request.options) {
        if (_known.count(option) == 0)
            continue;  // an engine that does not have it would only complain
        const auto applied = _applied.find(option);
        if (applied != _applied.end() && applied->second == value)
            continue;
        send("setoption name " + option + " value " + value);
        _applied[option] = value;
    }

    send(request.position);

    std::string go = "go";
    if (request.moveTimeMs > 0)
        go += " movetime " + std::to_string(request.moveTimeMs);
    if (request.depth > 0)
        go += " depth " + std::to_string(request.depth);
    if (request.moveTimeMs <= 0 && request.depth <= 0)
        go += " movetime 1000";
    send(go);

    _busy        = true;
    _discard     = false;
    _current     = SearchResult{};
    _current.tag = request.tag;
}

void Engine::readLoop(Process* process) {
    std::string line;
    while (process->readLine(line))
        handleLine(line);

    // The engine's output has ended, which means the engine has. Unless we
    // asked it to, that is a failure the game should hear about.
    std::lock_guard lock(_mutex);
    if (!_quitting) {
        if (_error.empty())
            _error = _state == State::Starting ? "the engine exited before it was ready"
                                               : "the engine exited unexpectedly";
        _state = State::Failed;
    }
    _busy = _discard = false;
    _pending.reset();
}

void Engine::handleLine(const std::string& line) {
    const auto token = words(line);
    if (token.empty())
        return;

    std::lock_guard lock(_mutex);
    if (_trace)
        _trace('<', line);

    if (token[0] == "id") {
        if (token.size() > 2 && token[1] == "name") {
            const std::size_t at = line.find("name ");
            _name = line.substr(at + 5);
        }
        return;
    }

    if (token[0] == "option") {
        // option name <words...> type <t> [default <v>] [min <a>] [max <b>]
        std::size_t i = 1;
        std::string name;
        if (i < token.size() && token[i] == "name") {
            for (++i; i < token.size() && token[i] != "type"; ++i)
                name += (name.empty() ? "" : " ") + token[i];
        }
        if (name.empty())
            return;
        _known.insert(name);

        std::string type;
        Spin spin;
        for (; i + 1 < token.size(); ++i) {
            if (token[i] == "type")
                type = token[i + 1];
            else if (token[i] == "default")
                toInt(token[i + 1], spin.value);
            else if (token[i] == "min")
                toInt(token[i + 1], spin.min);
            else if (token[i] == "max")
                toInt(token[i + 1], spin.max);
        }
        if (type == "spin")
            _spins[name] = spin;
        return;
    }

    if (token[0] == "uciok") {
        send("isready");
        return;
    }

    if (token[0] == "readyok") {
        if (_state == State::Starting) {
            _state = State::Ready;
            pump();
        }
        return;
    }

    if (token[0] == "info") {
        // "info string ..." is free text; anything in it that looks like a
        // field is a coincidence.
        if (!_busy || (token.size() > 1 && token[1] == "string"))
            return;

        int  depth = 0, multipv = 1;
        bool bound = false;
        std::optional<int> cp, mate;
        std::string firstMove;

        for (std::size_t i = 1; i < token.size(); ++i) {
            int value = 0;
            if (token[i] == "depth" && i + 1 < token.size() && toInt(token[i + 1], value)) {
                depth = value;
            } else if (token[i] == "multipv" && i + 1 < token.size() && toInt(token[i + 1], value)) {
                multipv = value;
            } else if (token[i] == "score" && i + 2 < token.size() && toInt(token[i + 2], value)) {
                if (token[i + 1] == "cp")
                    cp = value;
                else if (token[i + 1] == "mate")
                    mate = value;
            } else if (token[i] == "lowerbound" || token[i] == "upperbound") {
                bound = true;
            } else if (token[i] == "pv") {
                if (i + 1 < token.size())
                    firstMove = token[i + 1];
                break;  // everything after it is the line itself
            }
        }

        // Only a line that says something definite counts. Lines without a
        // score are progress chatter. A score marked as a bound is not a score
        // yet — the search failed high or low and is about to try again.
        if (bound || (!cp && !mate))
            return;

        // With several lines requested — by us, or by a strength limit behind
        // the scenes — line one is the best move and line two the next best.
        // The rest describe moves nobody asks about.
        if (multipv == 1) {
            _current.depth   = depth;
            _current.scoreCp = cp;
            _current.mateIn  = mate;
            _current.pvMove  = firstMove;
        } else if (multipv == 2) {
            _current.secondDepth = depth;
            _current.secondCp    = cp;
            _current.secondMate  = mate;
            _current.secondMove  = firstMove;
        }
        return;
    }

    if (token[0] == "bestmove") {
        if (!_busy)
            return;
        _busy = false;
        if (!_discard) {
            // "(none)" is what an engine says when there is nothing to play.
            if (token.size() > 1 && token[1] != "(none)" && token[1] != "0000")
                _current.bestMove = token[1];
            _results.push_back(std::move(_current));
        }
        _discard = false;
        pump();
        return;
    }
}

// --- finding the engine ----------------------------------------------------------

std::string executableDirectory() {
#if defined(__APPLE__)
    char raw[PATH_MAX];
    std::uint32_t size = sizeof(raw);
    if (_NSGetExecutablePath(raw, &size) != 0)
        return {};
    char resolved[PATH_MAX];
    const std::string path = ::realpath(raw, resolved) != nullptr ? resolved : raw;
#elif defined(_WIN32)
    const std::string path;
#else
    char raw[PATH_MAX];
    const ssize_t n = ::readlink("/proc/self/exe", raw, sizeof(raw) - 1);
    if (n <= 0)
        return {};
    const std::string path(raw, static_cast<std::size_t>(n));
#endif
    const std::size_t slash = path.find_last_of("/\\");
    return slash == std::string::npos ? std::string() : path.substr(0, slash);
}

std::string findEngine(const std::string& fallback) {
#if defined(_WIN32)
    (void)fallback;
    return {};
#else
    const auto usable = [](const std::string& path) {
        return !path.empty() && ::access(path.c_str(), X_OK) == 0;
    };

    if (const char* chosen = std::getenv("CHESS_ENGINE"); chosen != nullptr && usable(chosen))
        return chosen;

    const std::string beside = executableDirectory();
    if (!beside.empty() && usable(beside + "/stockfish"))
        return beside + "/stockfish";

    if (usable(fallback))
        return fallback;

    if (const char* searchPath = std::getenv("PATH"); searchPath != nullptr) {
        std::istringstream directories(searchPath);
        for (std::string directory; std::getline(directories, directory, ':');) {
            if (!directory.empty() && usable(directory + "/stockfish"))
                return directory + "/stockfish";
        }
    }
    return {};
#endif
}

}  // namespace chess
